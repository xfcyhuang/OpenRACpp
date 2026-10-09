// UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithDecorationBase.cs @b6fc03f
//          L18-146 + WithDecoration.cs L18-71
#include "game/actor_info.hpp"
#include "gfx/world_renderer.hpp"
#include "mods/render_sprites.hpp"
#include "mods/with_decoration.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

namespace {

std::vector<BlinkState> ParseBlinkPattern(const meta::GenericValue& gv_value) {
  std::vector<BlinkState> vec_out;
  if (const auto* arr = std::get_if<std::vector<meta::GenericValue>>(
          &gv_value.val))
    for (const meta::GenericValue& element : *arr)
      if (const auto* n = std::get_if<std::int64_t>(&element.val))
        vec_out.push_back(static_cast<BlinkState>(*n));
  return vec_out;
}

int2 ParseInt2Value(const meta::GenericValue& gv_value) {
  if (const auto* tuple = std::get_if<meta::GenericTuple>(&gv_value.val))
    if (tuple->uint1_count >= 2)
      return int2{static_cast<std::int32_t>(tuple->arr_ints[0]),
                  static_cast<std::int32_t>(tuple->arr_ints[1])};
  return int2{};
}

}  // namespace

WithDecorationBaseInfoData WithDecorationBaseInfoData::Parse(
    const meta::RecordObject& rec_info) {
  WithDecorationBaseInfoData data;
  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  if (const auto v = sim::RecordFieldString(rec_info, "Position"))
    data.str_position = std::string{*v};
  if (const auto* gv = sim::RecordFieldValue(rec_info, "ValidRelationships"))
    if (const auto* n = std::get_if<std::int64_t>(&gv->val))
      data.valid_relationships =
          static_cast<sim::PlayerRelationship>(*n);
  if (const auto v = sim::RecordFieldInt(rec_info, "RequiresSelection"))
    data.b_requires_selection = *v != 0;
  if (const auto* gv = sim::RecordFieldValue(rec_info, "Margin"))
    data.int2_margin = ParseInt2Value(*gv);
  if (const auto* gv = sim::RecordFieldValue(rec_info, "Offsets"))
    if (const auto* dict = std::get_if<meta::GenericDict>(&gv->val))
      for (const auto& [key, value] : *dict)
        if (const auto* str_key = std::get_if<std::string>(&key.val))
          data.vec_offsets.emplace_back(
              expr::BooleanExpression{*str_key}, ParseInt2Value(value));
  if (const auto v = sim::RecordFieldInt(rec_info, "BlinkInterval"))
    data.int4_blink_interval = static_cast<int>(*v);
  if (const auto* gv = sim::RecordFieldValue(rec_info, "BlinkPattern"))
    data.vec_blink_pattern = ParseBlinkPattern(*gv);
  if (const auto* gv = sim::RecordFieldValue(rec_info, "BlinkPatterns"))
    if (const auto* dict = std::get_if<meta::GenericDict>(&gv->val))
      for (const auto& [key, value] : *dict)
        if (const auto* str_key = std::get_if<std::string>(&key.val))
          data.vec_blink_patterns.emplace_back(
              expr::BooleanExpression{*str_key}, ParseBlinkPattern(value));
  return data;
}

std::vector<std::string> WithDecorationBaseInfoData::ConsumedConditionVariables()
    const {
  std::vector<std::string> vec_out;
  for (const auto& [expr, offset] : vec_offsets)
    for (const std::string& variable : expr.Variables())
      if (std::find(vec_out.begin(), vec_out.end(), variable) == vec_out.end())
        vec_out.push_back(variable);
  for (const auto& [expr, pattern] : vec_blink_patterns)
    for (const std::string& variable : expr.Variables())
      if (std::find(vec_out.begin(), vec_out.end(), variable) == vec_out.end())
        vec_out.push_back(variable);
  return vec_out;
}

// ———— WithDecoration ————

WithDecorationInfoData WithDecorationInfoData::Parse(
    const meta::RecordObject& rec_info) {
  WithDecorationInfoData data;
  data.base = WithDecorationBaseInfoData::Parse(rec_info);
  if (const auto v = sim::RecordFieldString(rec_info, "Image"))
    data.str_image = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "Sequence"))
    data.str_sequence = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "Palette"))
    data.str_palette = std::string{*v};
  if (const auto v = sim::RecordFieldInt(rec_info, "IsPlayerPalette"))
    data.b_is_player_palette = *v != 0;
  return data;
}

WithDecoration::WithDecoration(const ActorInitializer& init,
                               WithDecorationInfoData info)
    : WithDecorationBase<WithDecoration, WithDecorationInfoData>{
          init, std::move(info)} {
  Actor& self = init.Self();
  str_image_ = InfoData().str_image.empty()
                   ? std::string{self.Info()->Name()}
                   : InfoData().str_image;
  sim::World* ptr_world = &self.world();
  up_anim_ = std::make_unique<gfx::Animation>(
      RenderSprites::MakeAnimationDeps(self), str_image_,
      gfx::Animation::DefaultFacing(),
      [ptr_world]() { return ptr_world->Paused(); });
  up_anim_->PlayRepeating(InfoData().str_sequence);
}

gfx::PaletteReference* WithDecoration::GetPalette(Actor& self,
                                                  gfx::WorldRenderer& wr) {
  return wr.Palette(InfoData().b_is_player_palette
                        ? InfoData().str_palette +
                              self.Owner()->InternalName()
                        : InfoData().str_palette);
}

void WithDecoration::RenderDecorationAt(Actor& self, gfx::WorldRenderer& wr,
                                        int2 int2_screen_pos,
                                        std::vector<gfx::RenderItem>& vec_out) {
  const gfx::Sprite sprite = up_anim_->Image();
  const core::Vector2 vec_pos{
      static_cast<float>(int2_screen_pos.X) -
          0.5f * static_cast<float>(sprite.Bounds.Width),
      static_cast<float>(int2_screen_pos.Y) -
          0.5f * static_cast<float>(sprite.Bounds.Height)};
  vec_out.push_back(gfx::MakeUISpriteRenderable(
      sprite, self.CenterPosition(), vec_pos, 0, GetPalette(self, wr)));
}

}  // namespace ora::mods
