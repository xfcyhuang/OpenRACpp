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

struct PipDecorationSegment {
  int int4_pip_count = -1;

  int2 int2_pip_stride{};
  std::string str_image = "pips";
  std::string str_empty_sequence = "pip-empty";
  std::string str_full_sequence = "pip-green";
  std::string str_palette = "chrome";

  void ParseCommon(const meta::RecordObject& rec_info);
};

struct WithSpriteControlGroupDecorationInfoData {
  std::string str_palette = "chrome";
  std::string str_image = "pips";
  std::string str_group_sequence = "groups";
  std::string str_position = "TopLeft";
  int2 int2_margin{};

  static WithSpriteControlGroupDecorationInfoData Parse(
      const meta::RecordObject& rec_info);
};

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

  bool RequiresSelection() const override { return true; }

  void RenderDecoration(Actor& self, gfx::WorldRenderer& wr,
                        sim::ISelectionDecorations& container,
                        std::vector<gfx::RenderItem>& vec_out) override;

 private:
  WithSpriteControlGroupDecorationInfoData info_;
  std::unique_ptr<gfx::Animation> up_anim_;
};

struct WithResourceStoragePipsDecorationInfoData {
  WithDecorationBaseInfoData base;
  PipDecorationSegment pips;

  static WithResourceStoragePipsDecorationInfoData Parse(
      const meta::RecordObject& rec_info);
};

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

  void BindPlayerResources(sim::Player& owner);

  PlayerResources* ptr_player_resources_ = nullptr;
  std::unique_ptr<gfx::Animation> up_anim_;
};

struct WithStoresResourcesPipsDecorationInfoData {
  WithDecorationBaseInfoData base;
  PipDecorationSegment pips;

  std::vector<std::pair<std::string, std::string>> vec_resource_sequences;

  static WithStoresResourcesPipsDecorationInfoData Parse(
      const meta::RecordObject& rec_info);
};

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

  std::string_view GetPipSequence(int int4_index);

  void RenderDecorationAt(Actor& self, gfx::WorldRenderer& wr,
                          int2 int2_screen_pos,
                          std::vector<gfx::RenderItem>& vec_out) override;

 private:
  sim::IStoresResources* ptr_stores_resources_ = nullptr;
  std::unique_ptr<gfx::Animation> up_anim_;
};

}
