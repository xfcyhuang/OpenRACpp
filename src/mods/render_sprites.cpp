#include "mods/render_sprites.hpp"

#include "game/actor_info.hpp"
#include "gfx/world_renderer.hpp"
#include "map/map.hpp"
#include "mods/body_orientation.hpp"
#include "sim/actor_init.hpp"
#include "sim/player.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

namespace {

constexpr std::pair<sim::DamageState, std::string_view> kDamagePrefixes[] = {
    {sim::DamageState::Critical, "critical-"},
    {sim::DamageState::Heavy, "damaged-"},
    {sim::DamageState::Medium, "scratched-"},
    {sim::DamageState::Light, "scuffed-"},
};

std::optional<std::string> RecStringOpt(const meta::RecordObject& rec,
                                        std::string_view str_name) {
  if (const auto s = sim::RecordFieldString(rec, str_name))
    return std::string{*s};
  return std::nullopt;
}

}

void RenderSprites::AnimationWrapper::CachePalette(
    gfx::WorldRenderer& wr, const sim::Player& owner) {
  ptr_palette_reference = wr.Palette(b_is_player_palette
                                         ? str_palette + owner.InternalName()
                                         : str_palette);
}

void RenderSprites::AnimationWrapper::OwnerChanged() {

  if (b_is_player_palette)
    ptr_palette_reference = nullptr;
}

bool RenderSprites::AnimationWrapper::IsVisible() const {
  return ptr_animation->DisableFunc == nullptr ||
         !ptr_animation->DisableFunc();
}

bool RenderSprites::AnimationWrapper::Tick() {
  ptr_animation->AnimationRef->Tick();

  const bool b_visible = IsVisible();
  const WVec wvec_offset =
      ptr_animation->OffsetFunc ? ptr_animation->OffsetFunc() : WVec{};
  const gfx::ISpriteSequence* ptr_sequence =
      ptr_animation->AnimationRef->CurrentSequence();

  const bool b_updated = b_visible != b_cached_visible_ ||
                         wvec_offset != wvec_cached_offset_ ||
                         ptr_sequence != b_cached_sequence_;
  b_cached_visible_ = b_visible;
  wvec_cached_offset_ = wvec_offset;
  b_cached_sequence_ = ptr_sequence;

  return b_updated;
}

RenderSpritesInfoData RenderSpritesInfoData::Parse(
    const meta::RecordObject& rec_info) {
  RenderSpritesInfoData data;
  if (auto str = RecStringOpt(rec_info, "Image"))
    data.str_image = std::move(*str);
  if (const auto* gv = sim::RecordFieldValue(rec_info, "FactionImages"))
    if (const auto* map_dict = std::get_if<meta::GenericDict>(&gv->val))
      for (const auto& [key, value] : *map_dict)
        if (const auto* str_key = std::get_if<std::string>(&key.val))
          if (const auto* str_value = std::get_if<std::string>(&value.val))
            data.vec_faction_images.emplace_back(*str_key, *str_value);
  if (auto str = RecStringOpt(rec_info, "Palette"))
    data.str_palette = std::move(*str);
  if (auto str = RecStringOpt(rec_info, "PlayerPalette"))
    data.str_player_palette = std::move(*str);
  return data;
}

namespace {

std::string ToLowerInvariant(std::string str_value) {
  for (char& c : str_value)
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c - 'A' + 'a');
  return str_value;
}
}

std::string RenderSpritesInfoData::GetImage(
    const game::ActorInfo& actor, std::string_view str_faction) const {
  if (!vec_faction_images.empty() && !str_faction.empty())
    for (const auto& [faction, image] : vec_faction_images)
      if (faction == str_faction)
        return ToLowerInvariant(image);

  return ToLowerInvariant(str_image.empty() ? std::string{actor.Name()}
                                            : str_image);
}

gfx::Animation::Deps RenderSprites::MakeAnimationDeps(Actor& self) {
  return MakeAnimationDeps(self.world());
}

gfx::Animation::Deps RenderSprites::MakeAnimationDeps(sim::World& world) {
  gfx::Animation::Deps deps;
  deps.ptr_sequences = world.Map().Sequences();
  const map::Map* ptr_map = &world.Map();
  deps.fn_distance_above_terrain =
      [ptr_map](WPos pos) { return ptr_map->DistanceAboveTerrain(pos); };
  return deps;
}

std::function<WAngle()> RenderSprites::MakeFacingFunc(Actor& self) {

  sim::IFacing* ptr_facing = self.TraitOrDefault<sim::IFacing>();
  if (ptr_facing == nullptr)
    return [] { return WAngle{}; };

  return [ptr_facing] { return ptr_facing->Facing(); };
}

RenderSprites::RenderSprites(const ActorInitializer& init,
                             const RenderSpritesInfoData& info_data)
    : info_data_(info_data) {

  std::string str_faction =
      init.Self().Owner() != nullptr
          ? init.Self().Owner()->Faction().InternalName
          : std::string{};
  if (const auto* faction_init = init.GetOrDefault<sim::FactionInit>({}))
    str_faction = faction_init->Value();

  str_faction_ = std::move(str_faction);
}

const std::string& RenderSprites::GetImage(Actor& self) {
  if (!str_cached_image_.empty())
    return str_cached_image_;

  str_cached_image_ = info_data_.GetImage(*self.Info(), str_faction_);
  return str_cached_image_;
}

void RenderSprites::UpdatePalette() {
  b_should_refresh_palettes_ = true;
  for (auto& anim : vec_anims_)
    anim.OwnerChanged();
}

void RenderSprites::OnOwnerChanged(Actor& , sim::Player&,
                                   sim::Player&) {
  UpdatePalette();
}

void RenderSprites::OnEffectiveOwnerChanged(Actor& ,
                                            sim::Player&, sim::Player&) {
  UpdatePalette();
}

void RenderSprites::Render(Actor& self, gfx::WorldRenderer& wr,
                           std::vector<gfx::RenderItem>& vec_out) {

  if (b_should_refresh_palettes_) {
    b_should_refresh_palettes_ = false;
    for (auto& a : vec_anims_) {
      if (a.ptr_palette_reference == nullptr) {
        sim::Player* ptr_owner = self.Owner();
        if (self.EffectiveOwner() != nullptr &&
            self.EffectiveOwner()->Disguised())
          ptr_owner = self.EffectiveOwner()->Owner();
        a.CachePalette(wr, *ptr_owner);
      }
    }
  }

  RenderAnimations(self, vec_out);
}

void RenderSprites::RenderAnimations(Actor& self,
                                     std::vector<gfx::RenderItem>& vec_out) {

  for (auto& a : vec_anims_) {
    if (!a.IsVisible())
      continue;

    std::array<gfx::RenderItem, 2> arr_items{};
    const std::int32_t int4_count =
        a.ptr_animation->Render(self.CenterPosition(),
                                 a.ptr_palette_reference, arr_items);
    for (std::int32_t i = 0; i < int4_count; ++i)
      vec_out.push_back(arr_items[static_cast<std::size_t>(i)]);
  }
}

std::vector<Rectangle> RenderSprites::ScreenBounds(Actor& self,
                                                   gfx::WorldRenderer& wr) {

  std::vector<Rectangle> vec_bounds;
  for (auto& a : vec_anims_)
    if (a.IsVisible())
      vec_bounds.push_back(
          a.ptr_animation->AnimationRef->ScreenBounds(wr, self.CenterPosition(),
                                                      WVec{}));
  return vec_bounds;
}

void RenderSprites::Tick(Actor& self) {

  bool b_updated = false;
  for (auto& a : vec_anims_)
    b_updated |= a.Tick();

  if (b_updated)
    self.world().ScreenMapFace()->AddOrUpdate(&self);
}

void RenderSprites::Add(gfx::AnimationWithOffset& anim,
                        std::string_view str_palette,
                        bool b_is_player_palette) {

  AnimationWrapper wrapper;
  wrapper.ptr_animation = &anim;
  if (str_palette.empty()) {
    wrapper.str_palette = info_data_.str_palette.empty()
                              ? info_data_.str_player_palette
                              : info_data_.str_palette;
    wrapper.b_is_player_palette = info_data_.str_palette.empty();
  } else {
    wrapper.str_palette = std::string{str_palette};
    wrapper.b_is_player_palette = b_is_player_palette;
  }

  b_should_refresh_palettes_ = true;
  vec_anims_.push_back(std::move(wrapper));
}

void RenderSprites::Remove(gfx::AnimationWithOffset& anim) {

  std::erase_if(vec_anims_, [&anim](const AnimationWrapper& a) {
    return a.ptr_animation == &anim;
  });
}

std::string RenderSprites::UnnormalizeSequence(
    std::string_view str_sequence) {

  for (const auto& [state, prefix] : kDamagePrefixes) {
    if (str_sequence.starts_with(prefix)) {
      return std::string{
          str_sequence.substr(prefix.size())};
    }
  }

  return std::string{str_sequence};
}

std::string RenderSprites::NormalizeSequence(const gfx::Animation& anim,
                                             sim::DamageState state,
                                             std::string_view str_sequence) {

  std::string str_base = UnnormalizeSequence(str_sequence);

  for (const auto& [tier_state, prefix] : kDamagePrefixes) {
    const std::string str_prefixed =
        std::string{prefix} + str_base;
    if (state >= tier_state && anim.HasSequence(str_prefixed))
      return str_prefixed;
  }

  return str_base;
}

int2 RenderSprites::AutoRenderSize() const {

  for (const auto& b : vec_anims_) {
    if (b.IsVisible() && b.ptr_animation->AnimationRef->CurrentSequence() !=
                             nullptr) {
      gfx::Animation& anim = *b.ptr_animation->AnimationRef;
      const gfx::Sprite sprite = anim.Image();
      const float fp4_scale = anim.CurrentSequence()->Scale();
      return int2::FromVector(
          core::Vector2{static_cast<float>(sprite.Bounds.Width) * fp4_scale,
                        static_cast<float>(sprite.Bounds.Height) * fp4_scale});
    }
  }

  return int2{};
}

void RenderSprites::ModifyActorPreviewInit(Actor& ,
                                           sim::TypeDictionary& inits) {

  if (!inits.WithInterface<sim::FactionInit>().empty())
    return;

  up_owned_faction_init_ = std::make_unique<sim::FactionInit>(str_faction_);
  inits.Add(up_owned_faction_init_.get());
}

}
