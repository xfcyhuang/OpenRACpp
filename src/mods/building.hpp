// UPSTREAM: OpenRA.Mods.Common/Traits/Buildings/Building.cs @b6fc03f
//          L22-355 全文(LoadFootprint 的 yaml 面已由 game/loaders.cpp 的
//          RegisterLoadFootprint 前批落地 —— 本文件消费值袋槽)
//          The whole of Building.cs L22-355 (LoadFootprint's yaml face
//          already landed with game/loaders.cpp's RegisterLoadFootprint in
//          an earlier batch — this file consumes the value-bag slot).
//
// 机制对照 / Mechanism mapping:
//  - Footprint 的 FrozenDictionary<CVec, FootprintCellType> → 插入序
//    vector<pair<CVec, FootprintCellType>>(loader 的 dict 槽 = 插入序,
//    y 外 x 内的行主序;FootprintTiles 的枚举序 = 上游字典首建序等价)
//    Footprint's FrozenDictionary<CVec, FootprintCellType> → the
//    insertion-ordered vector<pair<CVec, FootprintCellType>> (the
//    loader's dict slot is insertion-ordered, row-major x-within-y;
//    FootprintTiles' enumeration order is the equivalent of upstream's
//    dictionary build order).
//  - IsCloseEnoughToBase 的 RequiresBuildableAreaInfo/MapBuildRadius/
//    BaseProvider/GivesBuildableArea 未移植:RequiresBuildableAreaInfo
//    == null 的真分支(上游无该 trait 世界同值;空集等价面 —— 登记
//    COVERAGE)
//    IsCloseEnoughToBase's RequiresBuildableAreaInfo/MapBuildRadius/
//    BaseProvider/GivesBuildableArea are unported: the true branch of
//    RequiresBuildableAreaInfo == null (the same value upstream gives in
//    a world without the trait; the empty-set equivalent face —
//    registered in COVERAGE).
//  - RemoveSmudges 走 SmudgeLayer 记账面(本批);BuildSounds/
//    UndeploySounds 的 Game.Sound.PlayToPlayer 随声音注入面
//    RemoveSmudges goes through SmudgeLayer's bookkeeping face (this
//    batch); BuildSounds/UndeploySounds' Game.Sound.PlayToPlayer rides
//    the sound injection face.
#pragma once
import std;

#include "core/cell_pos.hpp"
#include "core/cvec.hpp"
#include "core/wpos.hpp"
#include "sim/actor.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::sim {
class ActorInitializer;
class World;
}

namespace ora::game {
class ActorInfo;
}

namespace ora::mods {

/// Building.cs L22-29:FootprintCellType(char 码点即值)
/// Building.cs L22-29: FootprintCellType (the char code is the value).
enum class FootprintCellType : std::int32_t {
  Empty = '_',
  OccupiedPassable = '=',
  Occupied = 'x',
  OccupiedUntargetable = 'X',
  OccupiedPassableTransitOnly = '+',
};

/// BuildingInfo(L31-264)的解析面
/// The parse face of BuildingInfo (L31-264).
class BuildingInfoData {
 public:
  std::vector<std::string> vec_terrain_types;  // L34
  std::vector<std::pair<CVec, FootprintCellType>> vec_footprint{
      {CVec{0, 0}, FootprintCellType::Occupied}};  // L39(缺省 x@0,0)
  CVec dimensions{1, 1};                            // L41
  WVec local_center_offset{0, 0, 0};                // L44
  bool b_requires_base_provider = false;            // L46
  bool b_allow_invalid_placement = false;           // L48
  bool b_remove_smudges_on_build = true;            // L51
  bool b_remove_smudges_on_sell = true;             // L54
  bool b_remove_smudges_on_transform = true;        // L57
  std::vector<std::string> vec_build_sounds;        // L60
  std::vector<std::string> vec_undeploy_sounds;     // L62

  static BuildingInfoData Parse(const meta::RecordObject& rec_info);

  /// L97-100:FootprintTiles(指定类型的格;插入序)
  /// L97-100: FootprintTiles (the cells of the given type; insertion
  /// order).
  std::vector<CPos> FootprintTiles(CPos location,
                                   FootprintCellType type) const;

  /// L102-115:Tiles(四类型并集;= / x / X / + 序)
  /// L102-115: Tiles (the four-type union; the = / x / X / + order).
  std::vector<CPos> Tiles(CPos location) const;

  /// L117-124:FrozenUnderFogTiles(_ 先,再 Tiles)
  /// L117-124: FrozenUnderFogTiles (_ first, then Tiles).
  std::vector<CPos> FrozenUnderFogTiles(CPos location) const;

  /// L126-136:OccupiedTiles(x / X / +)
  /// L126-136: OccupiedTiles (x / X / +).
  std::vector<CPos> OccupiedTiles(CPos location) const;

  /// L138-145:PathableTiles(_ / =)
  /// L138-145: PathableTiles (_ / =).
  std::vector<CPos> PathableTiles(CPos location) const;

  /// L147-151:TransitOnlyTiles(+)
  /// L147-151: TransitOnlyTiles (+).
  std::vector<CPos> TransitOnlyTiles(CPos location) const;

  /// L153-157:CenterOffset
  /// L153-157: CenterOffset.
  WVec CenterOffset(sim::World& w) const;

  /// L159-181:FindBaseProvider(BaseProvider/MapBuildRadius 空集 → null)
  /// L159-181: FindBaseProvider (the empty BaseProvider/MapBuildRadius
  /// set → null).
  const void* FindBaseProvider(sim::World& /*world*/,
                               sim::Player& /*player*/,
                               CPos /*top_left*/) const {
    return nullptr;
  }

  /// L183-247:IsCloseEnoughToBase(RequiresBuildableAreaInfo 空集 →
  /// true;上游 L208-209 的真分支)
  /// L183-247: IsCloseEnoughToBase (the empty RequiresBuildableAreaInfo
  /// set → true; upstream's true branch of L208-209).
  bool IsCloseEnoughToBase(sim::World& /*world*/, sim::Player& /*player*/,
                           const game::ActorInfo& /*ai*/,
                           CPos /*top_left*/) const {
    return true;
  }

  /// L249-253:OccupiedCells(IOccupySpaceInfo 面;SubCell.FullCell)
  /// L249-253: OccupiedCells (the IOccupySpaceInfo face;
  /// SubCell.FullCell).
  std::vector<std::pair<CPos, SubCell>> OccupiedCellsInfo(
      CPos top_left) const;
};

/// Building(L266-355;IOccupySpace/ITargetableCells/ISync/通知四接口)
/// Building (L266-355; IOccupySpace/ITargetableCells/ISync/the four
/// notify interfaces).
class Building final : public sim::TraitBase,
                       public sim::IOccupySpace,
                       public sim::ITargetableCells,
                       public sim::ISync,
                       public sim::INotifyAddedToWorld,
                       public sim::INotifyRemovedFromWorld,
                       public sim::INotifySold,
                       public sim::INotifyTransform {
 public:
  ORA_TRAIT_INTERFACES(
      Building, OpenRA_Mods_Common_Traits_Building, sim::IOccupySpace,
      sim::ITargetableCells, sim::ISync, sim::INotifyAddedToWorld,
      sim::INotifyRemovedFromWorld, sim::INotifySold,
      sim::INotifyTransform)

  Building(sim::ActorInitializer& init, BuildingInfoData info);

  // ———— IOccupySpace ————
  WPos CenterPosition() const override { return center_position_; }
  CPos TopLeft() const override { return top_left_; }
  std::vector<std::pair<CPos, SubCell>> OccupiedCells() const override {
    return vec_occupied_cells_;
  }

  // ———— Building 自有 ————
  const std::vector<CPos>& TransitOnlyCells() const {
    return vec_transit_only_cells_;
  }
  const BuildingInfoData& Info() const { return info_; }

  /// L304:ITargetableCells.TargetableCells
  std::vector<std::pair<CPos, SubCell>> TargetableCells() override {
    return vec_targetable_cells_;
  }

  void AddedToWorld(sim::Actor& self) override;  // L306-318
  void RemovedFromWorld(sim::Actor& self) override;  // L320-324

  void Selling(sim::Actor& self) override;  // L326-329
  void Sold(sim::Actor& self) override {}   // L331

  void BeforeTransform(sim::Actor& self) override;  // L334-340
  void OnTransform(sim::Actor& self) override {}    // L342
  void AfterTransform(sim::Actor& to_actor) override {}  // L343

  /// L346-353:RemoveSmudges
  /// L346-353: RemoveSmudges.
  void RemoveSmudges(sim::Actor& self);

 private:
  BuildingInfoData info_;
  CPos top_left_;
  WPos center_position_;
  std::vector<std::pair<CPos, SubCell>> vec_occupied_cells_;
  std::vector<std::pair<CPos, SubCell>> vec_targetable_cells_;
  std::vector<CPos> vec_transit_only_cells_;
};

}  // namespace ora::mods
