#pragma once
import std;

#include "sim/trait_interfaces.hpp"

namespace ora::mods {

using sim::Actor;
using sim::ICreatePlayers;
using ora::MersenneTwister;
using sim::Player;
using sim::TraitBase;
using sim::World;

class Faction final : public TraitBase {
 public:

  static constexpr gen::TypeId kTypeId = gen::TypeId::OpenRA_Traits_Faction;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts =
      ora::sim::MakeTraitUpcasts<Faction>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }
};

class CreateMapPlayers final : public TraitBase, public ICreatePlayers {
 public:
  ORA_TRAIT_INTERFACES(CreateMapPlayers,
                       OpenRA_Mods_Common_Traits_CreateMapPlayers,
                       ICreatePlayers)

  void CreatePlayers(World& w, MersenneTwister& player_random) override;

 private:

  static void SetupPlayerMasks(Player* p, Player* q);
};

void RegisterCommonTraits();

void RegisterCommonTraitsBatch7();

void RegisterCommonTraitsBatch8();

void RegisterCommonTraitsBatch9();
void RegisterCommonTraitsBatch10();
void RegisterCommonTraitsBatch11();
void RegisterCommonTraitsBatch12();
void RegisterCommonTraitsBatch12Objectives();
void RegisterCommonTraitsBatch12Seeds();
void RegisterCommonTraitsBatch13();
void RegisterCommonTraitsBatch14();
void RegisterCommonTraitsBatch15();
void RegisterCommonTraitsBatch16();
void RegisterCommonTraitsBatch17();
void RegisterCommonTraitsBatch18();

}
