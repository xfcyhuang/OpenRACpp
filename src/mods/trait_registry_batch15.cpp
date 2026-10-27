#include "game/actor_info.hpp"
#include "meta/generic_record.hpp"
#include "mods/cargo.hpp"
#include "mods/parachutable.hpp"
#include "mods/paratroopers_power.hpp"
#include "mods/passenger.hpp"
#include "mods/with_parachute.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_registry.hpp"

namespace ora::mods {

void RegisterCommonTraitsBatch15() {
  auto& registry = sim::TraitRegistry::Instance();
  using ActorInitializer = sim::ActorInitializer;
  using sim::TraitBase;

  registry.Register(
      "CargoInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         ora::WorldArena& arena) -> TraitBase* {
        CargoInfoData data = CargoInfoData::Parse(rec_info);
        std::string str_instance_name;
        if (init.Self().Info() != nullptr)
          str_instance_name =
              std::string{init.Self().Info()->InstanceNameOf(&rec_info)};
        return arena.Create<Cargo>(init, data,
                                   std::move(str_instance_name));
      });

  registry.Register(
      "PassengerInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         ora::WorldArena& arena) -> TraitBase* {
        return arena.Create<Passenger>(PassengerInfoData::Parse(rec_info));
      });

  registry.Register(
      "ParaDropInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         ora::WorldArena& arena) -> TraitBase* {
        return arena.Create<ParaDrop>(init.Self(),
                                      ParaDropInfoData::Parse(rec_info));
      });

  registry.Register(
      "ParachutableInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         ora::WorldArena& arena) -> TraitBase* {
        return arena.Create<Parachutable>(
            init.Self(), ParachutableInfoData::Parse(rec_info));
      });

  registry.Register(
      "WithParachuteInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         ora::WorldArena& arena) -> TraitBase* {
        return arena.Create<WithParachute>(
            init, WithParachuteInfoData::Parse(rec_info));
      });

  registry.Register(
      "ParatroopersPowerInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         ora::WorldArena& arena) -> TraitBase* {
        return arena.Create<ParatroopersPower>(
            init, ParatroopersPowerInfoData::Parse(rec_info));
      });
}

}
