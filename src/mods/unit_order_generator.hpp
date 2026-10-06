// UPSTREAM: OpenRA.Mods.Common/Orders/OrderGenerator.cs @b6fc03f L18-61 +
//          Orders/UnitOrderGenerator.cs L21-227(逐语义重写 + 注入面)
//          Verbatim-semantics rewrite + the injection faces.
//
// 机制对照 / Mechanism mapping:
//  - OrderGenerator 基类的 ActionButton/CancelButton → GameSettingsFace 注入
//    (D101 Settings 批换实体);classic 样式 ctor 清空选择(L26-28)照抄
//    The base's ActionButton/CancelButton → the GameSettingsFace injection
//    (replaced in the D101 Settings batch); the classic-style ctor's
//    selection clear (L26-28) kept.
//  - TargetForInput 的 FrozenActorsAtMouse 分支(L46-51)与 FogObscures 过滤
//    (L40):FrozenActorLayer/Shroud 随其批 —— 部分覆盖装配面下 frozen 分支
//    恒空、fog 恒假(World.FogObscures 的注入面缺省;COVERAGE 登记)
//    The TargetForInput FrozenActorsAtMouse branch (L46-51) and the
//    FogObscures filter (L40): FrozenActorLayer/Shroud land with their
//    batches — under the partial-coverage assembly face the frozen branch
//    stays empty and fog stays false (the default of World.FogObscures's
//    injection face; in COVERAGE).
//  - WithHighestSelectionPriority(Exts.cs):IIssueOrder trait 未注册时空集
//    等价(上游扩展随 Selection trait 批接入 —— 本批先以最低优先序首达
//    形态承载,消耗面 = 空 actor 集)
//    WithHighestSelectionPriority: with no IIssueOrder traits registered
//    the empty-set equivalence holds (the upstream extension lands with
//    the Selection trait batch — carried this batch as a first-hit
//    lowest-priority form over the empty actor set).
//  - TextNotificationsManager.Debug(CheckSameOrder,L204-211)→ 注入钩子
//    (通知管理器随 Phase 6)| the injected hook (the notification manager
//    lands in Phase 6).
#pragma once
import std;

#include "game/input.hpp"
#include "sim/order_generator.hpp"
#include "sim/target.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

using sim::Actor;
using sim::IOrderGenerator;
using sim::IOrderTargeter;
using sim::IIssueOrder;
using ora::MersenneTwister;
using sim::TraitBase;
using sim::World;

/// UnitOrderResult(UnitOrderGenerator.cs L213-222) | UnitOrderResult
/// (UnitOrderGenerator.cs L213-222).
struct UnitOrderResult {
  sim::Actor* actor = nullptr;              // L215
  sim::IOrderTargeter* order = nullptr;     // L216(上游 Order 字段 = targeter)
  sim::IIssueOrder* trait = nullptr;        // L217
  std::string cursor;                       // L218(null → 空 | null as "")
  sim::Target target{};                     // L221
};

/// UnitOrderGenerator(UnitOrderGenerator.cs L21) | UnitOrderGenerator
/// (UnitOrderGenerator.cs L21).
class UnitOrderGenerator : public sim::IOrderGenerator {
 public:
  explicit UnitOrderGenerator(
      sim::World& world,
      const GameSettingsFace& game_settings = GameSettingsFace{});

  /// TargetForInput(L37-54) | TargetForInput (L37-54).
  static sim::Target TargetForInput(sim::World& world, CPos cell,
                                    int2 world_pixel,
                                    const MouseInput& mi);

  /// Order(L56-64) | Order (L56-64).
  std::vector<net::Order*> Order(sim::World& world, CPos cell,
                                 int2 world_pixel, const MouseInput& mi);

  /// OrderInner(L66-84) | OrderInner (L66-84).
  std::vector<net::Order*> OrderInner(sim::World& world, CPos cell,
                                      int2 world_pixel,
                                      const MouseInput& mi);

  // ———— IOrderGenerator 全量(IOrderGenerator.cs L20-27)————
  void Tick(sim::World& world) override;                                     // L86
  std::vector<gfx::IRenderable*> Render(sim::World& world) override;         // L88
  std::vector<gfx::IRenderable*> RenderAboveShroud(sim::World& world) override;  // L89
  std::vector<gfx::IRenderable*> RenderAnnotations(sim::World& world) override;  // L90
  std::string GetCursor(sim::World& world, CPos cell,
                        int2 world_pixel) override;                        // L92
  void Deactivate() override;                                                // L117
  bool HandleKeyPress() override { return false; }                           // L119
  void SelectionChanged(sim::World& world,
                        std::span<sim::Actor* const> selected) override;     // L156

  /// InputOverridesSelection(L122-154) | InputOverridesSelection
  /// (L122-154).
  bool InputOverridesSelection(sim::World& world, int2 xy, const MouseInput& mi);

  /// OrderForUnit(L163-202) | OrderForUnit (L163-202).
  std::optional<UnitOrderResult> OrderForUnit(sim::Actor* self,
                                              const sim::Target& target_in,
                                              CPos xy, const MouseInput& mi);

  /// ClearSelectionOnLeftClick/HasIssuedQueuedCommand(L224-225)
  /// (L224-225).
  bool ClearSelectionOnLeftClick() const { return b_clear_selection_on_left_click_; }
  bool HasIssuedQueuedCommand() const { return b_has_issued_queued_command_; }
  void SetHasIssuedQueuedCommand(bool b) { b_has_issued_queued_command_ = b; }

 private:
  /// CheckSameOrder(L204-211) | CheckSameOrder (L204-211).
  static net::Order* CheckSameOrder(sim::IOrderTargeter* iot, net::Order* order);

  std::string world_select_cursor_{"select"};    // L23(ChromeMetrics 面;
                                                 // 缺省键值 —— Chrome 装配批)
  std::string world_default_cursor_{"default"};  // L24(同上)
  GameSettingsFace game_settings_;               // L25(注入值面)
  bool b_clear_selection_on_left_click_ = true;  // L224
  bool b_has_issued_queued_command_ = false;     // L225
};

}  // namespace ora::mods
