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
  std::vector<std::string> vec_types;
  std::vector<std::string> vec_prerequisites;

  static WithProductionIconOverlayInfoData Parse(const meta::RecordObject& rec_info);
};

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
  std::string str_type;
  std::string str_image;
  std::string str_sequence;
  std::string str_palette{"chrome"};

  static ProductionIconOverlayManagerInfoData Parse(
      const meta::RecordObject& rec_info);
};

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

  void PrerequisitesAvailable(const std::string& key) override;

  void PrerequisitesUnavailable(const std::string& key) override;

  void PrerequisitesItemHidden(const std::string& key) override {}

  void PrerequisitesItemVisible(const std::string& key) override {}

  const gfx::Sprite* OverlaySprite() const override { return &sprite_; }
  std::string_view OverlayPalette() const override {
    return info_.str_palette;
  }
  core::Vector2 OverlayOffset(core::Vector2 vec_icon_size) const override;
  bool IsOverlayActive(const game::ActorInfo& ai) const override;

  const ProductionIconOverlayManagerInfoData& Info() const { return info_; }

 private:

  std::string MakeKey(std::string_view str_name) const;

  ProductionIconOverlayManagerInfoData info_;
  sim::Actor* p_self_ = nullptr;
  gfx::Sprite sprite_;
  std::string str_prefix_;
  std::map<const game::ActorInfo*, bool> map_overlay_active_;
};

}
