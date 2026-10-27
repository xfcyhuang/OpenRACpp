#pragma once
import std;

#include "core/int2.hpp"
#include "core/wangle.hpp"
#include "core/wvec.hpp"
#include "gfx/animation.hpp"
#include "gfx/gfx_util.hpp"
#include "gfx/renderable.hpp"
#include "meta/generic_record.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::gfx {
class WorldRenderer;
}

namespace ora::sim {
class World;
}

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

struct RenderSpritesInfoData {
  std::string str_image;
  std::vector<std::pair<std::string, std::string>> vec_faction_images;
  std::string str_palette;
  std::string str_player_palette = "player";

  static RenderSpritesInfoData Parse(const meta::RecordObject& rec_info);

  std::string GetImage(const game::ActorInfo& actor,
                       std::string_view str_faction) const;
};

class RenderSprites : public TraitBase,
                            public sim::ITick,
                            public sim::INotifyOwnerChanged,
                            public sim::INotifyEffectiveOwnerChanged,
                            public sim::IActorPreviewInitModifier,
                            public sim::IRender {
 public:

  struct AnimationWrapper {
    gfx::AnimationWithOffset* ptr_animation = nullptr;
    std::string str_palette;
    bool b_is_player_palette = false;
    gfx::PaletteReference* ptr_palette_reference = nullptr;

    bool b_cached_visible_ = false;
    WVec wvec_cached_offset_{};
    const gfx::ISpriteSequence* b_cached_sequence_ = nullptr;

    void CachePalette(gfx::WorldRenderer& wr, const sim::Player& owner);

    void OwnerChanged();

    bool IsVisible() const;

    bool Tick();
  };

  RenderSprites(const ActorInitializer& init,
                const RenderSpritesInfoData& info_data);

  ORA_TRAIT_INTERFACES(
      RenderSprites, OpenRA_Mods_Common_Traits_Render_RenderSprites,
      sim::ITick, sim::INotifyOwnerChanged,
      sim::INotifyEffectiveOwnerChanged, sim::IActorPreviewInitModifier,
      sim::IRender)

  const RenderSpritesInfoData& Info() const { return info_data_; }

  const std::vector<AnimationWrapper>& WrappersForTest() const {
    return vec_anims_;
  }

  static std::function<WAngle()> MakeFacingFunc(Actor& self);

  const std::string& GetImage(Actor& self);

  void UpdatePalette();

  void OnOwnerChanged(Actor& self, sim::Player& old_owner,
                      sim::Player& new_owner) override;
  void OnEffectiveOwnerChanged(Actor& self, sim::Player& old_effective_owner,
                               sim::Player& new_effective_owner) override;

  void Tick(Actor& self) override;

  void Add(gfx::AnimationWithOffset& anim, std::string_view str_palette = {},
           bool b_is_player_palette = false);

  void Remove(gfx::AnimationWithOffset& anim);

  static std::string UnnormalizeSequence(std::string_view str_sequence);

  static std::string NormalizeSequence(const gfx::Animation& anim,
                                       sim::DamageState state,
                                       std::string_view str_sequence);

  int2 AutoSelectionSize() const { return AutoRenderSize(); }
  int2 AutoRenderSize() const;

  void ModifyActorPreviewInit(Actor& self,
                              sim::TypeDictionary& inits) override;

  virtual void Render(Actor& self, gfx::WorldRenderer& wr,
                      std::vector<gfx::RenderItem>& vec_out);
  std::vector<Rectangle> ScreenBounds(Actor& self,
                                      gfx::WorldRenderer& wr) override;

  static gfx::Animation::Deps MakeAnimationDeps(Actor& self);
  static gfx::Animation::Deps MakeAnimationDeps(sim::World& world);

 private:

  void RenderAnimations(Actor& self, std::vector<gfx::RenderItem>& vec_out);

  RenderSpritesInfoData info_data_;
  std::string str_faction_;
  std::vector<AnimationWrapper> vec_anims_;

  bool b_should_refresh_palettes_ = false;
  std::string str_cached_image_;
  std::unique_ptr<sim::FactionInit>
      up_owned_faction_init_;

};

class RenderSpritesEditorOnly final : public RenderSprites {
 public:
  RenderSpritesEditorOnly(const ActorInitializer& init,
                          const RenderSpritesInfoData& info)
      : RenderSprites(init, info) {}

  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_Render_RenderSpritesEditorOnly;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts =
      ora::sim::MakeTraitUpcasts<RenderSpritesEditorOnly, RenderSprites,
                                 sim::ITick, sim::INotifyOwnerChanged,
                                 sim::INotifyEffectiveOwnerChanged,
                                 sim::IActorPreviewInitModifier, sim::IRender>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }

  void Render(Actor& self, gfx::WorldRenderer& wr,
              std::vector<gfx::RenderItem>& vec_out) override {

    (void)self;
    (void)wr;
    (void)vec_out;
  }
};

}
