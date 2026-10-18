// UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithParachute.cs @e180e3225
#include "mods/with_parachute.hpp"

#include "gfx/renderable.hpp"
#include "gfx/world_renderer.hpp"
#include "map/map.hpp"
#include "mods/body_orientation.hpp"
#include "mods/render_sprites.hpp"
#include "mods/render_utils.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

namespace {

std::optional<std::int64_t> FieldInt(const meta::RecordObject& rec_info,
                                     std::string_view str_name) {
  return sim::RecordFieldInt(rec_info, str_name);
}

std::optional<std::string> FieldString(const meta::RecordObject& rec_info,
                                       std::string_view str_name) {
  if (const auto v = sim::RecordFieldString(rec_info, str_name))
    return std::string{*v};
  return std::nullopt;
}

std::optional<bool> FieldBool(const meta::RecordObject& rec_info,
                              std::string_view str_name) {
  const auto* gv = sim::RecordFieldValue(rec_info, str_name);
  if (gv == nullptr)
    return std::nullopt;
  if (const auto* b = std::get_if<bool>(&gv->val))
    return *b;
  return std::nullopt;
}

std::optional<WVec> FieldWVec(const meta::RecordObject& rec_info,
                              std::string_view str_name) {
  const auto* gv = sim::RecordFieldValue(rec_info, str_name);
  if (gv == nullptr)
    return std::nullopt;
  if (const auto* tuple = std::get_if<meta::GenericTuple>(&gv->val))
    return WVec{static_cast<std::int32_t>(tuple->arr_ints[0]),
                static_cast<std::int32_t>(tuple->arr_ints[1]),
                static_cast<std::int32_t>(tuple->arr_ints[2])};
  return std::nullopt;
}

std::optional<core::Color> FieldColor(const meta::RecordObject& rec_info,
                                      std::string_view str_name) {
  const auto* gv = sim::RecordFieldValue(rec_info, str_name);
  if (gv == nullptr)
    return std::nullopt;
  if (const auto* n = std::get_if<std::int64_t>(&gv->val))
    return core::Color::FromArgbRaw(static_cast<std::uint32_t>(*n));
  return std::nullopt;
}

}  // namespace

WithParachuteInfoData WithParachuteInfoData::Parse(
    const meta::RecordObject& rec_info) {
  WithParachuteInfoData data;
  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  if (const auto v = FieldString(rec_info, "Image"))
    data.str_image = *v;
  if (const auto v = FieldString(rec_info, "OpeningSequence"))
    data.str_opening_sequence = *v;
  if (const auto v = FieldString(rec_info, "Sequence"))
    data.str_sequence = *v;
  if (const auto v = FieldString(rec_info, "ClosingSequence"))
    data.str_closing_sequence = *v;
  if (const auto v = FieldString(rec_info, "Palette"))
    data.str_palette = *v;
  if (const auto v = FieldBool(rec_info, "IsPlayerPalette"))
    data.b_is_player_palette = *v;
  if (const auto v = FieldWVec(rec_info, "Offset"))
    data.wvec_offset = *v;
  if (const auto v = FieldString(rec_info, "ShadowImage"))
    data.str_shadow_image = *v;
  if (const auto v = FieldString(rec_info, "ShadowSequence"))
    data.str_shadow_sequence = *v;
  if (const auto v = FieldColor(rec_info, "ShadowColor"))
    data.color_shadow = *v;
  if (const auto v = FieldWVec(rec_info, "ShadowOffset"))
    data.wvec_shadow_offset = *v;
  if (const auto v = FieldInt(rec_info, "ShadowZOffset"))
    data.int4_shadow_z_offset = static_cast<int>(*v);
  return data;
}

WithParachute::WithParachute(const sim::ActorInitializer& init,
                             WithParachuteInfoData info)
    : sim::ConditionalTraitCore<WithParachute>(info.conditional),
      info_{std::move(info)} {
  Actor& self = init.Self();

  if (!info_.str_shadow_image.empty()) {
    opt_shadow_.emplace(
        RenderSprites::MakeAnimationDeps(self.world()),
        info_.str_shadow_image);
    opt_shadow_->PlayRepeating(info_.str_shadow_sequence);
  }

  vec_shadow_color_ = core::Vector3{
      static_cast<float>(info_.color_shadow.R()) / 255.0f,
      static_cast<float>(info_.color_shadow.G()) / 255.0f,
      static_cast<float>(info_.color_shadow.B()) / 255.0f};
  fp4_shadow_alpha_ =
      static_cast<float>(info_.color_shadow.A()) / 255.0f;

  if (info_.str_image.empty())
    return;

  ptr_body_ = self.Trait<BodyOrientation>();
  ptr_render_sprites_ = self.Trait<RenderSprites>();
}

void WithParachute::Created(sim::Actor& self) {
  sim::ConditionalTraitCore<WithParachute>::CoreCreated(self);

  if (info_.str_image.empty())
    return;

  opt_overlay_.emplace(RenderSprites::MakeAnimationDeps(self.world()),
                       info_.str_image);

  BodyOrientation* ptr_body = ptr_body_;
  const WithParachuteInfoData* ptr_info = &info_;

  opt_anim_.emplace(
      *opt_overlay_,
      [ptr_body, ptr_info, &self]() {
        return ptr_body->LocalToWorld(
            ptr_info->wvec_offset.Rotate(
                ptr_body->QuantizeOrientation(self.Orientation())));
      },
      [this]() { return IsTraitDisabled() && !b_render_prolonged_; },
      [&self, ptr_info](WPos pos) {
        return render::ZOffsetFromCenter(self, pos, 1);
      });

  ptr_render_sprites_->Add(*opt_anim_, info_.str_palette,
                           info_.b_is_player_palette);
}

void WithParachute::Tick(sim::Actor&) {
  if (opt_shadow_.has_value())
    opt_shadow_->Tick();
}

void WithParachute::Render(sim::Actor& self, gfx::WorldRenderer& wr,
                           std::vector<gfx::RenderItem>& vec_out) {
  if (!opt_shadow_.has_value())
    return;

  if (IsTraitDisabled() || self.IsDead() || !self.IsInWorld())
    return;

  if (self.world().FogObscures(self))
    return;

  gfx::Animation& shadow = *opt_shadow_;
  const WDist dat = self.world().Map().DistanceAboveTerrain(
      self.CenterPosition());
  const WPos pos = self.CenterPosition() - WVec{0, 0, dat.Length};
  const gfx::PaletteReference* ptr_palette = wr.Palette(info_.str_palette);
  const float fp4_alpha =
      shadow.CurrentSequence()->GetAlpha(shadow.CurrentFrame());
  const gfx::TintModifiers kind_tint_mods =
      shadow.CurrentSequence()->IgnoreWorldTint()
          ? gfx::TintModifiers::ReplaceColor |
                gfx::TintModifiers::IgnoreWorldTint
          : gfx::TintModifiers::ReplaceColor;
  const gfx::Sprite sprite_image = shadow.Image();
  const WAngle wangle_rotation{};
  vec_out.push_back(gfx::MakeSpriteRenderable(
      sprite_image, pos, info_.wvec_shadow_offset,
      info_.int4_shadow_z_offset, ptr_palette, 1.0f,
      fp4_shadow_alpha_ * fp4_alpha, vec_shadow_color_, kind_tint_mods,
      true, wangle_rotation));
}

std::vector<Rectangle> WithParachute::ScreenBounds(sim::Actor& self,
                                                   gfx::WorldRenderer& wr) {
  std::vector<Rectangle> vec_bounds;
  if (!opt_shadow_.has_value())
    return vec_bounds;

  if (IsTraitDisabled() || self.IsDead() || !self.IsInWorld())
    return vec_bounds;

  if (self.world().FogObscures(self))
    return vec_bounds;

  const WDist dat = self.world().Map().DistanceAboveTerrain(
      self.CenterPosition());
  const WPos pos = self.CenterPosition() - WVec{0, 0, dat.Length};
  vec_bounds.push_back(
      opt_shadow_->ScreenBounds(wr, pos, info_.wvec_shadow_offset));
  return vec_bounds;
}

void WithParachute::TraitEnabledHook(sim::Actor&) {
  if (info_.str_image.empty())
    return;

  opt_anim_->AnimationRef->PlayThen(
      info_.str_opening_sequence, [this]() {
        opt_anim_->AnimationRef->PlayRepeating(info_.str_sequence);
      });
}

void WithParachute::TraitDisabledHook(sim::Actor&) {
  if (info_.str_image.empty())
    return;

  b_render_prolonged_ = true;
  if (!info_.str_closing_sequence.empty())
    opt_anim_->AnimationRef->PlayThen(
        info_.str_closing_sequence,
        [this]() { b_render_prolonged_ = false; });
  else
    opt_anim_->AnimationRef->PlayBackwardsThen(
        info_.str_opening_sequence,
        [this]() { b_render_prolonged_ = false; });
}

}  // namespace ora::mods
