// UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithProductionIconOverlay.cs
//          @1f594e320 L17-40 + Traits/Render/ProductionIconOverlayManager.cs
//          L19-126 全文
//          WithProductionIconOverlay.cs L17-40 + the whole of
//          ProductionIconOverlayManager.cs L19-126.
#pragma once
import std;

#include "gfx/sprite.hpp"
#include "sim/actor.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::gfx {
class Animation;
}

namespace ora::meta {
class RecordObject;
}

namespace ora::game {
class ActorInfo;
class Ruleset;
}

namespace ora::sim {
class ActorInitializer;
}

namespace ora::mods {

struct WithProductionIconOverlayInfoData {
  std::vector<std::string> vec_types;          // L24
  std::vector<std::string> vec_prerequisites;  // L27

  static WithProductionIconOverlayInfoData Parse(const meta::RecordObject& rec_info);
};

/// WithProductionIconOverlay(L39;空运行时类)
/// WithProductionIconOverlay (L39; the empty runtime class).
class WithProductionIconOverlay final : public sim::TraitBase {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_Render_WithProductionIconOverlay;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts =
      ora::sim::MakeTraitUpcasts<WithProductionIconOverlay>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }
};

struct ProductionIconOverlayManagerInfoData {
  std::string str_type;      // L32
  std::string str_image;     // L35
  std::string str_sequence;  // L38
  std::string str_palette{"chrome"};  // L41

  static ProductionIconOverlayManagerInfoData Parse(
      const meta::RecordObject& rec_info);
};

/// ProductionIconOverlayManager(L59-125;player actor 挂载,ITechTreeElement)
/// ProductionIconOverlayManager (L59-125; player-actor mounted,
/// ITechTreeElement).
class ProductionIconOverlayManager final : public sim::TraitBase,
                                           public sim::ITechTreeElement,
                                           public sim::IProductionIconOverlay {
 public:
  ORA_TRAIT_INTERFACES(
      ProductionIconOverlayManager,
      OpenRA_Mods_Common_Traits_Render_ProductionIconOverlayManager,
      sim::ITechTreeElement, sim::IProductionIconOverlay)

  ProductionIconOverlayManager(sim::ActorInitializer& init,
                               ProductionIconOverlayManagerInfoData info);

  /// L96-106 | L96-106.
  void PrerequisitesAvailable(const std::string& key) override;
  /// L108-111 | L108-111.
  void PrerequisitesUnavailable(const std::string& key) override;
  /// L113-114 | L113-114.
  void PrerequisitesItemHidden(const std::string& key) override {}
  /// L113-114 | L113-114.
  void PrerequisitesItemVisible(const std::string& key) override {}

  const gfx::Sprite* OverlaySprite() const override { return &sprite_; }
  std::string_view OverlayPalette() const override {
    return info_.str_palette;
  }
  core::Vector2 OverlayOffset(core::Vector2 vec_icon_size) const override;
  bool IsOverlayActive(const game::ActorInfo& ai) const override;

  const ProductionIconOverlayManagerInfoData& Info() const { return info_; }

 private:
  /// L94 | L94.
  std::string MakeKey(std::string_view str_name) const;

  ProductionIconOverlayManagerInfoData info_;
  sim::Actor* p_self_ = nullptr;
  gfx::Sprite sprite_;                             // L62
  std::string str_prefix_;                         // L71
  std::map<const game::ActorInfo*, bool> map_overlay_active_;  // L73
};

}  // namespace ora::mods
