#include "mods/spawn_map_actors.hpp"

#include "map/map.hpp"
#include "meta/parse.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/player.hpp"
#include "sim/type_dictionary.hpp"
#include "sim/world.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::mods {

namespace {

std::string_view YamlText(const yaml::MiniYaml& yaml_value) {
  return yaml_value.Value != nullptr ? std::string_view{*yaml_value.Value}
                                     : std::string_view{};
}
}

void RegisterCommonActorInits() {
  static const bool b_registered = [] {
    sim::InitRegistry& registry = sim::InitRegistry::Instance();

    registry.Register(
        "Location",
        [](const yaml::MiniYaml& yaml_value,
           const std::string&) -> sim::ActorInit* {
          CPos pos;
          if (!meta::TryParseCPosNet(YamlText(yaml_value), pos))
            throw std::runtime_error(
                "Invalid CPos value '" + std::string{YamlText(yaml_value)} +
                "'");
          return new sim::LocationInit(pos);
        });

    registry.Register(
        "Owner",
        [](const yaml::MiniYaml& yaml_value,
           const std::string&) -> sim::ActorInit* {
          return new sim::OwnerInit(std::string{YamlText(yaml_value)});
        });

    registry.Register(
        "Facing",
        [](const yaml::MiniYaml& yaml_value,
           const std::string&) -> sim::ActorInit* {
          std::int32_t degrees = 0;
          if (!meta::TryParseInt32Invariant(YamlText(yaml_value), degrees))
            throw std::runtime_error(
                "Invalid WAngle value '" +
                std::string{YamlText(yaml_value)} + "'");
          return new sim::FacingInit(WAngle{degrees});
        });

    registry.Register(
        "SubCell",
        [](const yaml::MiniYaml& yaml_value,
           const std::string&) -> sim::ActorInit* {
          std::uint8_t value = 0;
          if (!meta::TryParseByteInvariant(YamlText(yaml_value), value))
            throw std::runtime_error(
                "Invalid SubCell value '" +
                std::string{YamlText(yaml_value)} + "'");
          return new sim::SubCellInit(static_cast<sim::SubCell>(value));
        });

    registry.Register(
        "TurretFacing",
        [](const yaml::MiniYaml& yaml_value,
           const std::string& str_instance_name) -> sim::ActorInit* {
          std::int32_t degrees = 0;
          if (!meta::TryParseInt32Invariant(YamlText(yaml_value), degrees))
            throw std::runtime_error(
                "Invalid WAngle value '" +
                std::string{YamlText(yaml_value)} + "'");
          return new sim::TurretFacingInit(WAngle{degrees},
                                           str_instance_name);
        });

    registry.Register(
        "Health",
        [](const yaml::MiniYaml& yaml_value,
           const std::string&) -> sim::ActorInit* {
          std::int32_t value = 0;
          if (!meta::TryParseInt32Invariant(YamlText(yaml_value), value))
            throw std::runtime_error(
                "Invalid Health value '" +
                std::string{YamlText(yaml_value)} + "'");
          return new sim::HealthInit(value);
        });

    return true;
  }();
  (void)b_registered;
}

void SpawnMapActors::WorldLoaded(sim::World& world, gfx::WorldRenderer*) {

  for (const yaml::MiniYamlNode& kv : world.Map().ActorDefinitions()) {
    const std::string str_actor_type =
        kv.Value.Value != nullptr ? *kv.Value.Value : std::string{};

    sim::TypeDictionary init_dict;
    std::vector<std::unique_ptr<sim::ActorInit>> vec_owned_inits;

    for (const yaml::MiniYamlNode& init_node : kv.Value.Nodes) {
      const std::string str_key =
          init_node.Key != nullptr ? *init_node.Key : std::string{};

      const std::size_t at = str_key.find('@');
      const std::string str_init_name =
          at == std::string::npos ? str_key : str_key.substr(0, at);
      const std::string str_instance_name =
          at == std::string::npos ? std::string{} : str_key.substr(at + 1);

      vec_owned_inits.emplace_back(std::unique_ptr<sim::ActorInit>(
          sim::InitRegistry::Instance().CreateOrThrow(
              str_init_name, init_node.Value, str_instance_name)));
      init_dict.Add(vec_owned_inits.back().get());
    }

    if (sim::OwnerInit* owner_init =
            init_dict.GetOrDefault<sim::OwnerInit>()) {
      bool valid = false;
      for (const sim::Player* p : world.Players())
        if (p->InternalName() == owner_init->InternalName())
          valid = true;
      if (!valid) {

        std::vector<sim::OwnerInit*> vec_owner_inits =
            init_dict.WithInterface<sim::OwnerInit>();
        for (sim::OwnerInit* existing : vec_owner_inits)
          init_dict.Remove(existing);
        vec_owned_inits.emplace_back(std::make_unique<sim::OwnerInit>(
            world.WorldActor()->Owner()->InternalName()));
        init_dict.Add(vec_owned_inits.back().get());
      }
    }

    vec_owned_inits.emplace_back(std::make_unique<sim::SkipMakeAnimsInit>());
    init_dict.Add(vec_owned_inits.back().get());
    vec_owned_inits.emplace_back(std::make_unique<sim::SpawnedByMapInit>());
    init_dict.Add(vec_owned_inits.back().get());

    sim::Actor* actor = world.CreateActor(true, str_actor_type, init_dict);
    map_actors_[kv.Key != nullptr ? *kv.Key : std::string{}] = actor;
    uint4_last_map_actor_id_ = actor->ActorID();
  }
}

}
