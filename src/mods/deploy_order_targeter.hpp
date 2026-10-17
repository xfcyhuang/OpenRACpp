// UPSTREAM: OpenRA.Mods.Common/Orders/DeployOrderTargeter.cs @e180e3225
//          L18-46 全文
//          The whole of DeployOrderTargeter.cs L18-46.
#pragma once
import std;

#include "sim/target.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

/// DeployOrderTargeter(L18-45)
class DeployOrderTargeter final : public sim::IOrderTargeter {
 public:
  DeployOrderTargeter(std::string str_order, int int4_priority,
                      std::function<std::string()> fn_cursor)
      : str_order_id_{std::move(str_order)},
        int4_order_priority_{int4_priority},
        fn_cursor_{std::move(fn_cursor)} {}

  std::string OrderID() const override { return str_order_id_; }
  int OrderPriority() const override { return int4_order_priority_; }

  bool TargetOverridesSelection(sim::Actor&, const sim::Target&,
                                std::span<sim::Actor* const>, CPos,
                                sim::TargetModifiers) override {
    return true;
  }

  bool CanTarget(sim::Actor& self, const sim::Target& target,
                 sim::TargetModifiers& modifiers,
                 std::string& cursor) override {
    if (target.type != sim::TargetType::Actor)
      return false;

    b_is_queued_ = sim::HasModifier(modifiers, sim::TargetModifiers::ForceQueue);
    cursor = fn_cursor_ ? fn_cursor_() : std::string{};

    return &self == target.ActorPtr;
  }

  bool IsQueued() const override { return b_is_queued_; }

 private:
  std::string str_order_id_;
  int int4_order_priority_;
  std::function<std::string()> fn_cursor_;
  bool b_is_queued_ = false;
};

}  // namespace ora::mods
