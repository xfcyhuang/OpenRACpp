// UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithSpriteControlGroupDecoration.cs
//          @b6fc03f L18-69 + WithResourceStoragePipsDecoration.cs L18-79 +
//          WithStoresResourcesPipsDecoration.cs L18-101 pip/控制组装饰
//          (AmmoPips/CargoPips 随宿主 trait 批)。
//          The pip/control-group decorations (AmmoPips/CargoPips ride their
//          host traits' batches).
#pragma once
import std;

#include "gfx/animation.hpp"
#include "meta/generic_record.hpp"
#include "mods/with_decoration.hpp"
#include "sim/actor.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

/// 三个 Info 的共用 pip 段(PipCount/PipStride/Image/Empty/Full/Palette)
/// The shared pip segment of the three Infos (PipCount/PipStride/Image/
/// Empty/Full/Palette).
struct PipDecorationSegment {
  int int4_pip_count = -1;  // Ammo/ResourceStorage 用 -1 缺省;两个
                            // [FieldLoader.Require] 端解析侧校验
  int2 int2_pip_stride{};   // 零 = pip 宽步进
  std::string str_image = "pips";
  std::string str_empty_sequence = "pip-empty";
  std::string str_full_sequence = "pip-green";
  std::string str_palette = "chrome";

  /// 通用字段读取(子类特有字段由各自 Parse 追加)
  /// The common field reads (a subclass's own fields append in its Parse).
  void ParseCommon(const meta::RecordObject& rec_info);
};

/// WithSpriteControlGroupDecorationInfo(L21-37):非条件 trait(基
/// WithDecorationBaseInfo 之外独立承载)
/// WithSpriteControlGroupDecorationInfo (L21-37): a non-conditional trait
/// (carried independently of the WithDecorationBaseInfo base).
struct WithSpriteControlGroupDecorationInfoData {
  std::string str_palette = "chrome";  // L24
  std::string str_image = "pips";      // L27
  std::string str_group_sequence = "groups";  // L31
  std::string str_position = "TopLeft";       // L34
  int2 int2_margin{};                         // L37

  static WithSpriteControlGroupDecorationInfoData Parse(
      const meta::RecordObject& rec_info);
};

/// WithSpriteControlGroupDecoration(L39-69):控制组像素装饰
/// WithSpriteControlGroupDecoration (L39-69): the control-group pixel
/// decoration.
class WithSpriteControlGroupDecoration final : public TraitBase,
                                               public sim::IDecoration {
 public:
  WithSpriteControlGroupDecoration(const ActorInitializer& init,
                                   WithSpriteControlGroupDecorationInfoData
                                       info);

  ORA_TRAIT_INTERFACES(
      WithSpriteControlGroupDecoration,
      OpenRA_Mods_Common_Traits_Render_WithSpriteControlGroupDecoration,
      sim::IDecoration)

  /// L50:恒需选中
  /// L50: always requires selection.
  bool RequiresSelection() const override { return true; }

  /// L52-63:PlayFetchIndex 按组号取帧
  /// L52-63: PlayFetchIndex fetches the frame by the group number.
  void RenderDecoration(Actor& self, gfx::WorldRenderer& wr,
                        sim::ISelectionDecorations& container,
                        std::vector<gfx::RenderItem>& vec_out) override;

 private:
  WithSpriteControlGroupDecorationInfoData info_;
  std::unique_ptr<gfx::Animation> up_anim_;
};

/// WithResourceStoragePipsDecorationInfo(L20-44)
struct WithResourceStoragePipsDecorationInfoData {
  WithDecorationBaseInfoData base;
  PipDecorationSegment pips;

  static WithResourceStoragePipsDecorationInfoData Parse(
      const meta::RecordObject& rec_info);
};

/// WithResourceStoragePipsDecoration(L46-79):仓库存量 pips
/// WithResourceStoragePipsDecoration (L46-79): the storage-fill pips.
class WithResourceStoragePipsDecoration final
    : public WithDecorationBase<WithResourceStoragePipsDecoration,
                                WithResourceStoragePipsDecorationInfoData>,
      public sim::INotifyOwnerChanged {
 public:
  WithResourceStoragePipsDecoration(
      const ActorInitializer& init,
      WithResourceStoragePipsDecorationInfoData info);

  ORA_TRAIT_INTERFACES(
      WithResourceStoragePipsDecoration,
      OpenRA_Mods_Common_Traits_Render_WithResourceStoragePipsDecoration,
      sim::IObservesVariables, sim::IDecoration, sim::INotifyOwnerChanged)

  void OnOwnerChanged(Actor& self, sim::Player& old_owner,
                      sim::Player& new_owner) override;

 protected:
  void RenderDecorationAt(Actor& self, gfx::WorldRenderer& wr,
                          int2 int2_screen_pos,
                          std::vector<gfx::RenderItem>& vec_out) override;

 private:
  /// L71-74:换主重取 PlayerResources
  /// L71-74: re-fetch PlayerResources on the owner change.
  void BindPlayerResources(sim::Player& owner);

  PlayerResources* ptr_player_resources_ = nullptr;
  std::unique_ptr<gfx::Animation> up_anim_;
};

/// WithStoresResourcesPipsDecorationInfo(L21-49)
struct WithStoresResourcesPipsDecorationInfoData {
  WithDecorationBaseInfoData base;
  PipDecorationSegment pips;
  /// ResourceSequences(L39-41):[资源类型 → 序列名] 插入序对
  /// ResourceSequences (L39-41): the [resource type → sequence]
  /// insertion-ordered pairs.
  std::vector<std::pair<std::string, std::string>> vec_resource_sequences;

  static WithStoresResourcesPipsDecorationInfoData Parse(
      const meta::RecordObject& rec_info);
};

/// WithStoresResourcesPipsDecoration(L51-101):载具存量 pips(资源类型分色)
/// WithStoresResourcesPipsDecoration (L51-101): the carrier's fill pips
/// (per-resource-type colors).
class WithStoresResourcesPipsDecoration final
    : public WithDecorationBase<WithStoresResourcesPipsDecoration,
                                WithStoresResourcesPipsDecorationInfoData> {
 public:
  WithStoresResourcesPipsDecoration(
      const ActorInitializer& init,
      WithStoresResourcesPipsDecorationInfoData info);

  ORA_TRAIT_INTERFACES(
      WithStoresResourcesPipsDecoration,
      OpenRA_Mods_Common_Traits_Render_WithStoresResourcesPipsDecoration,
      sim::IObservesVariables, sim::IDecoration)

 protected:
  /// GetPipSequence(L68-87):Contents 前缀和的桶定位
  /// GetPipSequence (L68-87): the bucket lookup over Contents's prefix
  /// sums.
  std::string_view GetPipSequence(int int4_index);

  void RenderDecorationAt(Actor& self, gfx::WorldRenderer& wr,
                          int2 int2_screen_pos,
                          std::vector<gfx::RenderItem>& vec_out) override;

 private:
  sim::IStoresResources* ptr_stores_resources_ = nullptr;
  std::unique_ptr<gfx::Animation> up_anim_;
};

}  // namespace ora::mods
