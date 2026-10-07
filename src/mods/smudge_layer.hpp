// UPSTREAM: OpenRA.Mods.Common/Traits/World/SmudgeLayer.cs @b6fc03f
//          L26-121(Info + LoadInitialSmudges)+ L123-133/163-186/188-196
//          (SmudgeLayer 的同步记账面:AddSmudge/RemoveSmudge/tiles·dirty)
//          The Info + LoadInitialSmudges of L26-121 + the synchronous
//          bookkeeping face of L123-133/163-186/188-196 (AddSmudge/
//          RemoveSmudge/tiles·dirty).
//
// 机制对照 / Mechanism mapping:
//  - 渲染面(TerrainSpriteLayer/paletteReference/IRenderOverlay.Render/
//    ITickRender 的可见性推进/WorldLoaded 的 InitialSmudges 上传)随
//    渲染批;同步面 = tiles/dirty 两字典记账(Type/Depth;删除判据 =
//    上游 Sequence==null 的 bool 化)
//    The render faces (the TerrainSpriteLayer/paletteReference/
//    IRenderOverlay.Render/ITickRender's visibility advance/WorldLoaded's
//    InitialSmudges upload) land with the render batch; the synchronous
//    face = the tiles/dirty dictionary bookkeeping (Type/Depth; the
//    deletion marker = a bool of upstream's Sequence==null).
//  - AddSmudge 的 CosmeticRandom 消耗(smoke 概率 + smudges.Keys.Random)
//    与 depth 增殖(需序列帧长)未接:上游走 Game.CosmeticRandom(非同步
//    RNG,不进 SyncHash/replay),烟效与深度增殖为纯视觉差异;smudge 类型
//    退化为 Info.Type 单选(登记 COVERAGE)
//    AddSmudge's CosmeticRandom consumptions (the smoke probability +
//    smudges.Keys.Random) and the depth increment (which needs the
//    sequence's frame count) are unwired: upstream rides
//    Game.CosmeticRandom (a non-synced RNG that never enters SyncHash/
//    replays), so the smoke effect and the depth growth are purely visual
//    differences; the smudge type degrades to the single Info.Type pick
//    (registered in COVERAGE).
//  - CellEntryChanged 的四订阅(Tiles/CustomTerrain/Ramp/Height)随地图
//    事件面(当前 Map 无变更写者;RemoveUnacceptableSmudgeOnCellChange
//    的消费为零 —— 等价;登记)
//    The four CellEntryChanged subscriptions (Tiles/CustomTerrain/Ramp/
//    Height) ride the map-event face (the current Map carries no writers,
//    so RemoveUnacceptableSmudgeOnCellChange has zero consumers — the
//    equivalent; registered).
#pragma once
import std;

#include "core/cell_pos.hpp"
#include "sim/actor.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::sim {
class ActorInitializer;
}

namespace ora::mods {

/// SmudgeLayer.cs L22-26:MapSmudge
/// SmudgeLayer.cs L22-26: MapSmudge.
struct MapSmudge {
  std::string str_type;
  int int4_depth = 0;
};

/// SmudgeLayerInfo(L28-73)的解析面
/// The parse face of SmudgeLayerInfo (L28-73).
struct SmudgeLayerInfoData {
  std::string str_type{"Scorch"};      // L30
  std::string str_sequence{"scorch"};  // L33
  int int4_smoke_chance = 0;           // L36
  WDist max_smoke_offset_distance{0};  // L39
  std::string str_smoke_image;         // L42
  std::vector<std::string> vec_smoke_sequences;  // L46
  std::string str_smoke_palette{"effect"};       // L50
  std::string str_palette{"terrain"};            // L53 TerrainPaletteInternalName
  std::vector<std::pair<CPos, MapSmudge>> vec_initial_smudges;  // L56

  static SmudgeLayerInfoData Parse(const meta::RecordObject& rec_info);
};

/// SmudgeLayer(L123-274;同步记账面)
/// SmudgeLayer (L123-274; the synchronous bookkeeping face).
class SmudgeLayer final : public sim::TraitBase,
                          public sim::INotifyActorDisposing {
 public:
  ORA_TRAIT_INTERFACES(
      SmudgeLayer, OpenRA_Mods_Common_Traits_SmudgeLayer,
      sim::INotifyActorDisposing)

  SmudgeLayer(sim::Actor& self, SmudgeLayerInfoData info);

  /// L163-186:AddSmudge(记账面;dirty 的 Sequence==null = 删除判据)
  /// L163-186: AddSmudge (the bookkeeping face; dirty's Sequence==null
  /// is the deletion marker).
  void AddSmudge(CPos loc);

  /// L188-196:RemoveSmudge
  /// L188-196: RemoveSmudge.
  void RemoveSmudge(CPos loc);

  const SmudgeLayerInfoData& Info() const { return info_; }

 private:
  /// L198-208:RemoveUnacceptableSmudgeOnCellChange(订阅面空集;等价体)
  /// L198-208: RemoveUnacceptableSmudgeOnCellChange (the subscription
  /// face is empty; the equivalent body).

  void Disposing(sim::Actor& self) override;  // L258-273

  struct Smudge {
    std::string str_type;
    int int4_depth = 0;
    bool b_deleted = false;  // 上游 Sequence==null 判据的 bool 化
                             // upstream's Sequence==null marker as a bool.
  };

  SmudgeLayerInfoData info_;
  sim::World& world_;
  std::map<CPos, Smudge> map_tiles_;   // tiles(插入序无关;按 CPos 有序)
  std::map<CPos, Smudge> map_dirty_;   // dirty(TickRender 半段消费)
  bool b_disposed_ = false;
};

}  // namespace ora::mods
