// UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithSpriteControlGroupDecoration.cs
//          @b6fc03f(statement-by-statement;机制对照见
//          with_decoration_pips.hpp)
//          Statement-by-statement; the mechanism mapping lives in
//          with_decoration_pips.hpp.
#include "gfx/world_renderer.hpp"
#include "mods/player_resources.hpp"
#include "mods/render_sprites.hpp"
#include "mods/with_decoration_pips.hpp"
#include "sim/control_groups.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

namespace {

int2 ParseInt2Value(const meta::GenericValue& gv_value) {
  if (const auto* tuple = std::get_if<meta::GenericTuple>(&gv_value.val))
    if (tuple->uint1_count >= 2)
      return int2{static_cast<std::int32_t>(tuple->arr_ints[0]),
                  static_cast<std::int32_t>(tuple->arr_ints[1])};
  return int2{};
}

/// 上游 pip 行的共用绘制段:逐 pip 步进 screenPos
/// The common draw segment of a pip row: screenPos advances per pip.
void DrawPipRow(gfx::Animation& anim_pips, Actor& self,
                const PipDecorationSegment& seg_pips, gfx::WorldRenderer& wr,
                int2 int2_screen_pos,
                const std::function<std::string_view(int)>& fn_sequence,
                std::vector<gfx::RenderItem>& vec_out) {
  anim_pips.PlayRepeating(seg_pips.str_empty_sequence);
  gfx::PaletteReference* ptr_palette = wr.Palette(seg_pips.str_palette);
  const gfx::Sprite sprite_first = anim_pips.Image();
  const int2 int2_pip_size{sprite_first.Bounds.Width,
                           sprite_first.Bounds.Height};
  const int2 int2_pip_stride =
      seg_pips.int2_pip_stride != int2{}
          ? seg_pips.int2_pip_stride
          : int2{int2_pip_size.X, 0};
  int2_screen_pos = int2_screen_pos - int2{int2_pip_size.X / 2,
                                           int2_pip_size.Y / 2};

  for (int i = 0; i < seg_pips.int4_pip_count; ++i) {
    anim_pips.PlayRepeating(std::string{fn_sequence(i)});
    const gfx::Sprite sprite = anim_pips.Image();
    vec_out.push_back(gfx::MakeUISpriteRenderable(
        sprite, self.CenterPosition(),
        core::Vector2{static_cast<float>(int2_screen_pos.X),
                      static_cast<float>(int2_screen_pos.Y)},
        0, ptr_palette));
    int2_screen_pos = int2_screen_pos + int2_pip_stride;
  }
}

}  // namespace

void PipDecorationSegment::ParseCommon(const meta::RecordObject& rec_info) {
  if (const auto v = sim::RecordFieldInt(rec_info, "PipCount"))
    int4_pip_count = static_cast<int>(*v);
  if (const auto* gv = sim::RecordFieldValue(rec_info, "PipStride"))
    int2_pip_stride = ParseInt2Value(*gv);
  if (const auto v = sim::RecordFieldString(rec_info, "Image"))
    str_image = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "EmptySequence"))
    str_empty_sequence = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "FullSequence"))
    str_full_sequence = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "Palette"))
    str_palette = std::string{*v};
}

// ———— WithSpriteControlGroupDecoration ————

WithSpriteControlGroupDecorationInfoData
WithSpriteControlGroupDecorationInfoData::Parse(
    const meta::RecordObject& rec_info) {
  WithSpriteControlGroupDecorationInfoData data;
  if (const auto v = sim::RecordFieldString(rec_info, "Palette"))
    data.str_palette = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "Image"))
    data.str_image = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "GroupSequence"))
    data.str_group_sequence = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "Position"))
    data.str_position = std::string{*v};
  if (const auto* gv = sim::RecordFieldValue(rec_info, "Margin"))
    data.int2_margin = ParseInt2Value(*gv);
  return data;
}

WithSpriteControlGroupDecoration::WithSpriteControlGroupDecoration(
    const ActorInitializer& init,
    WithSpriteControlGroupDecorationInfoData info)
    : info_{std::move(info)} {
  up_anim_ = std::make_unique<gfx::Animation>(
      RenderSprites::MakeAnimationDeps(init.Self()), info_.str_image,
      gfx::Animation::DefaultFacing(), [] { return false; });
}

void WithSpriteControlGroupDecoration::RenderDecoration(
    Actor& self, gfx::WorldRenderer& wr,
    sim::ISelectionDecorations& container,
    std::vector<gfx::RenderItem>& vec_out) {
  const std::optional<int> opt_group =
      self.world().ControlGroups()->GetControlGroupForActor(&self);
  if (!opt_group.has_value())
    return;

  up_anim_->PlayFetchIndex(
      info_.str_group_sequence,
      [group = *opt_group]() { return group; });
  const gfx::Sprite sprite = up_anim_->Image();
  const int2 int2_half{sprite.Bounds.Width / 2, sprite.Bounds.Height / 2};
  const int2 int2_screen_pos =
      container.GetDecorationOrigin(self, wr, info_.str_position,
                                    info_.int2_margin) -
      int2_half;
  gfx::PaletteReference* ptr_palette = wr.Palette(info_.str_palette);
  vec_out.push_back(gfx::MakeUISpriteRenderable(
      sprite, self.CenterPosition(),
      core::Vector2{static_cast<float>(int2_screen_pos.X),
                    static_cast<float>(int2_screen_pos.Y)},
      0, ptr_palette));
}

// ———— WithResourceStoragePipsDecoration ————

WithResourceStoragePipsDecorationInfoData
WithResourceStoragePipsDecorationInfoData::Parse(
    const meta::RecordObject& rec_info) {
  WithResourceStoragePipsDecorationInfoData data;
  data.base = WithDecorationBaseInfoData::Parse(rec_info);
  data.pips.ParseCommon(rec_info);
  return data;
}

void WithResourceStoragePipsDecoration::BindPlayerResources(
    sim::Player& owner) {
  ptr_player_resources_ = owner.PlayerActor()->Trait<PlayerResources>();
}

WithResourceStoragePipsDecoration::WithResourceStoragePipsDecoration(
    const ActorInitializer& init,
    WithResourceStoragePipsDecorationInfoData info)
    : WithDecorationBase<WithResourceStoragePipsDecoration,
                         WithResourceStoragePipsDecorationInfoData>{
          init, std::move(info)} {
  BindPlayerResources(*init.Self().Owner());
  up_anim_ = std::make_unique<gfx::Animation>(
      RenderSprites::MakeAnimationDeps(init.Self()),
      InfoData().pips.str_image, gfx::Animation::DefaultFacing(),
      [] { return false; });
}

void WithResourceStoragePipsDecoration::OnOwnerChanged(
    Actor& self, sim::Player& old_owner, sim::Player& new_owner) {
  // L71-74
  (void)self;
  (void)old_owner;
  BindPlayerResources(new_owner);
}

void WithResourceStoragePipsDecoration::RenderDecorationAt(
    Actor& self, gfx::WorldRenderer& wr, int2 int2_screen_pos,
    std::vector<gfx::RenderItem>& vec_out) {
  // 满判定 = 整数交叉乘法(Resources*PipCount > i*Capacity)
  // The full test = integer cross-multiplication.
  const int int4_resources = ptr_player_resources_->Resources;
  const int int4_capacity = ptr_player_resources_->ResourceCapacity;
  const PipDecorationSegment& seg_pips = InfoData().pips;
  DrawPipRow(*up_anim_, self, seg_pips, wr, int2_screen_pos,
             [&](int int4_index) -> std::string_view {
               return int4_resources * seg_pips.int4_pip_count >
                              int4_index * int4_capacity
                          ? std::string_view{seg_pips.str_full_sequence}
                          : std::string_view{seg_pips.str_empty_sequence};
             },
             vec_out);
}

// ———— WithStoresResourcesPipsDecoration ————

WithStoresResourcesPipsDecorationInfoData
WithStoresResourcesPipsDecorationInfoData::Parse(
    const meta::RecordObject& rec_info) {
  WithStoresResourcesPipsDecorationInfoData data;
  data.base = WithDecorationBaseInfoData::Parse(rec_info);
  data.pips.ParseCommon(rec_info);
  if (const auto* gv = sim::RecordFieldValue(rec_info, "ResourceSequences"))
    if (const auto* dict = std::get_if<meta::GenericDict>(&gv->val))
      for (const auto& [key, value] : *dict)
        if (const auto* str_key = std::get_if<std::string>(&key.val))
          if (const auto* str_value = std::get_if<std::string>(&value.val))
            data.vec_resource_sequences.emplace_back(*str_key, *str_value);
  return data;
}

WithStoresResourcesPipsDecoration::WithStoresResourcesPipsDecoration(
    const ActorInitializer& init,
    WithStoresResourcesPipsDecorationInfoData info)
    : WithDecorationBase<WithStoresResourcesPipsDecoration,
                         WithStoresResourcesPipsDecorationInfoData>{
          init, std::move(info)} {
  std::vector<sim::IStoresResources*> vec_stores =
      init.Self().TraitsImplementing<sim::IStoresResources>();
  if (vec_stores.empty())
    throw std::runtime_error("Sequence contains no elements");
  ptr_stores_resources_ = vec_stores.front();
  up_anim_ = std::make_unique<gfx::Animation>(
      RenderSprites::MakeAnimationDeps(init.Self()),
      InfoData().pips.str_image, gfx::Animation::DefaultFacing(),
      [] { return false; });
}

std::string_view WithStoresResourcesPipsDecoration::GetPipSequence(
    int int4_index) {
  // L68-87:n = i*Capacity/PipCount;Contents 前缀和减法定位资源桶
  // n = i*Capacity/PipCount;Contents 前缀和定位资源桶
  // n = i*Capacity/PipCount; Contents's prefix sums locate the bucket.
  const int int4_pip_count = InfoData().pips.int4_pip_count;
  int int4_n = int4_index * ptr_stores_resources_->Capacity() /
               int4_pip_count;
  for (const auto& [str_type, int4_value] :
       ptr_stores_resources_->Contents()) {
    if (int4_n < int4_value) {
      for (const auto& [str_key, str_sequence] :
           InfoData().vec_resource_sequences)
        if (str_key == str_type)
          return str_sequence;
      return InfoData().pips.str_full_sequence;
    }
    int4_n -= int4_value;
  }
  return InfoData().pips.str_empty_sequence;
}

void WithStoresResourcesPipsDecoration::RenderDecorationAt(
    Actor& self, gfx::WorldRenderer& wr, int2 int2_screen_pos,
    std::vector<gfx::RenderItem>& vec_out) {
  DrawPipRow(*up_anim_, self, InfoData().pips, wr, int2_screen_pos,
             [this](int int4_index) { return GetPipSequence(int4_index); },
             vec_out);
}

}  // namespace ora::mods
