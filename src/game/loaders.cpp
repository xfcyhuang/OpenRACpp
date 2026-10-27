import std;
#include "game/game_records.hpp"
#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "meta/parse.hpp"
#include "yaml/mini_yaml.hpp"
#include "core/cell_pos.hpp"
#include "core/text.hpp"

namespace ora::game {

namespace {

std::shared_ptr<meta::GeneratedRecord> MakeRecord(std::string_view str_full_name) {

  const meta::RecordDesc* desc = meta::TypeRegistry::FindType(str_full_name);
  auto rec = meta::TypeRegistry::CreateObject(str_full_name);
  if (desc == nullptr || rec == nullptr)
    throw std::runtime_error(std::format("MakeRecord: type not registered: {}", str_full_name));
  return std::shared_ptr<meta::GeneratedRecord>(
      static_cast<meta::GeneratedRecord*>(rec.release()));
}

std::shared_ptr<meta::GeneratedRecord> MakeLoadedRecord(std::string_view str_full_name,
                                                        const yaml::MiniYaml& yaml_node) {
  auto rec = MakeRecord(str_full_name);
  meta::Load(rec.get(), yaml_node);
  return rec;
}

std::string_view KeyOf(const yaml::MiniYamlNode& node) {
  return node.Key != nullptr ? std::string_view{*node.Key} : std::string_view{};
}

std::string_view ValueTextOf(const yaml::MiniYaml& yaml_v) {
  return yaml_v.Value != nullptr ? std::string_view{*yaml_v.Value} : std::string_view{};
}

void RegisterLoadSpeeds(std::string_view str_composite) {
  meta::RegisterGeneratedLoader(
      str_composite,
      [](meta::GenericValue& val_slot, const yaml::MiniYaml& yaml_trait) {
        const yaml::MiniYamlNode& node_speeds =
            yaml_trait.NodeWithKey("TerrainSpeeds");
        meta::GenericDict map_ret;
        for (const yaml::MiniYamlNode& node_terrain : node_speeds.Value.Nodes) {
          const std::int32_t int4_speed = meta::GetInt32Value(
              "speed", ValueTextOf(node_terrain.Value));
          if (int4_speed <= 0)
            continue;

          std::int16_t int2_cost{};
          if (const yaml::MiniYamlNode* node_cost =
                  node_terrain.Value.NodeWithKeyOrDefault("PathingCost")) {
            int2_cost = static_cast<std::int16_t>(
                meta::GetInt32Value("cost", ValueTextOf(node_cost->Value)));
          } else {
            int2_cost = static_cast<std::int16_t>(10000 / int4_speed);
          }

          map_ret.emplace_back(
              meta::GenericValue::Of(std::string{KeyOf(node_terrain)}),
              meta::GenericValue::Of(meta::GenericTuple{
                  .arr_ints = {int4_speed, int2_cost, 0, 0}, .arr_floats = {},
                  .uint1_count = 2}));
        }
        val_slot = meta::GenericValue::Of(std::move(map_ret));
      });
}

constexpr bool IsFootprintCellDefined(std::int32_t int4_code) {
  return int4_code == '_' || int4_code == '=' || int4_code == 'x' ||
         int4_code == 'X' || int4_code == '+';
}

void RegisterLoadFootprint(std::string_view str_composite) {
  meta::RegisterGeneratedLoader(
      str_composite,
      [](meta::GenericValue& val_slot, const yaml::MiniYaml& yaml_trait) {

        std::vector<std::int32_t> vec_chars{'x'};
        if (const yaml::MiniYamlNode* node_fp =
                yaml_trait.NodeWithKeyOrDefault("Footprint")) {
          vec_chars.clear();
          for (const char ch : ValueTextOf(node_fp->Value))
            if (!std::isspace(static_cast<unsigned char>(ch)))
              vec_chars.push_back(static_cast<std::int32_t>(ch) & 0xFF);
        }

        std::int32_t int4_dimX{1}, int4_dimY{1};
        if (const yaml::MiniYamlNode* node_dims =
                yaml_trait.NodeWithKeyOrDefault("Dimensions")) {
          const std::vector<std::string_view> vec_parts =
              meta::SplitCommaTrimmed(ValueTextOf(node_dims->Value));
          if (vec_parts.size() == 2 &&
              meta::TryParseInt32Invariant(vec_parts[0], int4_dimX) &&
              meta::TryParseInt32Invariant(vec_parts[1], int4_dimY)) {

          } else {
            int4_dimX = 1;
            int4_dimY = 1;
          }
        }

        if (static_cast<std::int64_t>(vec_chars.size()) !=
            static_cast<std::int64_t>(int4_dimX) * int4_dimY) {
          const std::string str_fp =
              yaml_trait.NodeWithKeyOrDefault("Footprint") != nullptr
                  ? std::string{ValueTextOf(yaml_trait.NodeWithKeyOrDefault("Footprint")->Value)}
                  : "x";
          throw yaml::YamlException(std::format(
              "Invalid footprint: {} does not match dimensions {}x{}", str_fp,
              int4_dimX, int4_dimY));
        }

        meta::GenericDict map_ret;
        std::size_t int4_index{};
        for (std::int32_t int4_y{}; int4_y < int4_dimY; int4_y++) {
          for (std::int32_t int4_x{}; int4_x < int4_dimX; int4_x++) {
            const std::int32_t int4_c = vec_chars[int4_index++];
            if (!IsFootprintCellDefined(int4_c))
              throw yaml::YamlException(
                  std::format("Invalid footprint cell type '{}'",
                              static_cast<char>(int4_c)));
            map_ret.emplace_back(
                meta::GenericValue::Of(meta::GenericTuple{
                    .arr_ints = {int4_x, int4_y, 0, 0}, .arr_floats = {},
                    .uint1_count = 2}),
                meta::GenericValue::Of(std::int64_t{int4_c}));
          }
        }
        val_slot = meta::GenericValue::Of(std::move(map_ret));
      });
}

void RegisterLoadResourceTypes(std::string_view str_composite,
                               std::string_view str_record_name) {
  meta::RegisterGeneratedLoader(
      str_composite,
      [str_record_name](meta::GenericValue& val_slot,
                        const yaml::MiniYaml& yaml_trait) {
        meta::GenericDict map_ret;
        if (const yaml::MiniYamlNode* node_types =
                yaml_trait.NodeWithKeyOrDefault("ResourceTypes")) {
          for (const yaml::MiniYamlNode& node_r : node_types->Value.Nodes)
            map_ret.emplace_back(
                meta::GenericValue::Of(std::string{KeyOf(node_r)}),
                meta::GenericValue::Of(
                    MakeLoadedRecord(str_record_name, node_r.Value)));
        }
        val_slot = meta::GenericValue::Of(std::move(map_ret));
      });
}

void RegisterLoadInitialSmudges(std::string_view str_composite) {
  meta::RegisterGeneratedLoader(
      str_composite,
      [](meta::GenericValue& val_slot, const yaml::MiniYaml& yaml_trait) {
        meta::GenericDict map_ret;
        if (const yaml::MiniYamlNode* node_smudges =
                yaml_trait.NodeWithKeyOrDefault("InitialSmudges")) {
          for (const yaml::MiniYamlNode& node : node_smudges->Value.Nodes) {
            try {
              CPos cell_pos{};
              if (!meta::TryParseCPosNet(KeyOf(node), cell_pos))
                continue;
              const std::vector<std::string_view> vec_parts =
                  meta::SplitComma(ValueTextOf(node.Value));
              if (vec_parts.size() < 2)
                continue;
              const std::int32_t int4_depth =
                  meta::GetInt32Value("depth", vec_parts[1]);

              meta::GenericDict map_smudge;
              map_smudge.emplace_back(meta::GenericValue::Of(std::string{"Type"}),
                                      meta::GenericValue::Of(std::string{vec_parts[0]}));
              map_smudge.emplace_back(meta::GenericValue::Of(std::string{"Depth"}),
                                      meta::GenericValue::Of(std::int64_t{int4_depth}));
              map_ret.emplace_back(
                  meta::GenericValue::Of(meta::GenericTuple{
                      .arr_ints = {cell_pos.X(), cell_pos.Y(),
                                   std::int64_t{cell_pos.Layer()}, 0},
                      .arr_floats = {}, .uint1_count = 3}),
                  meta::GenericValue::Of(std::move(map_smudge)));
            } catch (...) {

            }
          }
        }
        val_slot = meta::GenericValue::Of(std::move(map_ret));
      });
}

void RegisterLoadShape(std::string_view str_composite) {
  meta::RegisterGeneratedLoader(
      str_composite,
      [](meta::GenericValue& val_slot, const yaml::MiniYaml& yaml_trait) {
        const yaml::MiniYamlNode* node_shape =
            yaml_trait.NodeWithKeyOrDefault("Type");
        const std::string str_shape =
            node_shape != nullptr ? std::string{ValueTextOf(node_shape->Value)}
                                  : std::string{};

        if (!str_shape.empty()) {
          try {
            auto rec = MakeRecord(std::format("{}Shape", str_shape));
            meta::Load(rec.get(), node_shape->Value);
            val_slot = meta::GenericValue::Of(std::move(rec));
          } catch (const yaml::YamlException& e_except) {
            throw yaml::YamlException(
                std::format("HitShape {}: {}", str_shape, e_except.what()));
          }
        } else {

          val_slot = meta::GenericValue::Of(MakeRecord("OpenRA.Mods.Common.HitShapes.CircleShape"));
        }

      });
}

void RegisterLoadDecisions(std::string_view str_composite) {
  meta::RegisterGeneratedLoader(
      str_composite,
      [](meta::GenericValue& val_slot, const yaml::MiniYaml& yaml_trait) {
        std::vector<meta::GenericValue> vec_ret;
        if (const yaml::MiniYamlNode* node_decisions =
                yaml_trait.NodeWithKeyOrDefault("Decisions")) {
          for (const yaml::MiniYamlNode& node_d : node_decisions->Value.Nodes)
            vec_ret.push_back(meta::GenericValue::Of(
                MakeLoadedRecord("OpenRA.Mods.Common.Traits.SupportPowerDecision",
                                 node_d.Value)));
        }
        val_slot = meta::GenericValue::Of(std::move(vec_ret));
      });
}

std::vector<meta::GenericValue> LoadOptionsImpl(const yaml::MiniYaml& yaml_trait) {
  std::vector<meta::GenericValue> vec_ret;
  const yaml::MiniYamlNode* node_options =
      yaml_trait.NodeWithKeyOrDefault("Options");
  if (node_options == nullptr)
    return vec_ret;

  for (const yaml::MiniYamlNode& node : node_options->Value.Nodes) {
    const std::string_view sv_key = KeyOf(node);
    const std::size_t int4_at = sv_key.find('@');
    if (int4_at == std::string_view::npos)
      continue;
    const std::string_view sv_kind = sv_key.substr(0, int4_at);

    std::string_view sv_record;
    if (sv_kind == "BooleanOption")
      sv_record = "OpenRA.Mods.Common.MapGenerator.MapGeneratorBooleanOption";
    else if (sv_kind == "IntegerOption")
      sv_record = "OpenRA.Mods.Common.MapGenerator.MapGeneratorIntegerOption";
    else if (sv_kind == "MultiIntegerChoiceOption")
      sv_record =
          "OpenRA.Mods.Common.MapGenerator.MapGeneratorMultiIntegerChoiceOption";
    else if (sv_kind == "MultiChoiceOption")
      sv_record = "OpenRA.Mods.Common.MapGenerator.MapGeneratorMultiChoiceOption";
    else
      continue;

    vec_ret.push_back(
        meta::GenericValue::Of(MakeLoadedRecord(sv_record, node.Value)));
  }
  return vec_ret;
}

void AppendFluentReferences(const meta::GeneratedRecord& rec_option,
                            std::vector<std::string>& vec_out) {
  const meta::RecordDesc& desc = rec_option.record_desc();
  const std::vector<const meta::FieldDesc*> vec_fields = meta::CollectFields(desc);
  const std::vector<meta::GenericValue>& vec_values = rec_option.Values();
  for (std::size_t int4_i{}; int4_i < vec_fields.size(); int4_i++) {
    if (vec_fields[int4_i]->str_name == "Label" &&
        std::holds_alternative<std::string>(vec_values[int4_i].val))
      vec_out.push_back(std::get<std::string>(vec_values[int4_i].val));
  }

  if (desc.str_name == "MapGeneratorMultiChoiceOption") {
    for (std::size_t int4_i{}; int4_i < vec_fields.size(); int4_i++) {
      if (vec_fields[int4_i]->str_name != "Choices")
        continue;
      if (std::holds_alternative<meta::GenericDict>(vec_values[int4_i].val)) {
        for (const auto& [val_key, val_choice] :
             std::get<meta::GenericDict>(vec_values[int4_i].val)) {
          (void)val_key;
          const auto* rec_choice =
              std::get_if<std::shared_ptr<meta::RecordObject>>(&val_choice.val);
          if (rec_choice == nullptr || *rec_choice == nullptr)
            continue;
          const auto* rec_generated =
              dynamic_cast<const meta::GeneratedRecord*>(rec_choice->get());
          if (rec_generated == nullptr)
            continue;
          AppendFluentReferences(*rec_generated, vec_out);
        }
      }
    }
  }
}

void RegisterMapGeneratorLoaders(std::string_view str_type_name) {
  meta::RegisterGeneratedLoader(
      std::format("{}.LoadOptions", str_type_name),
      [](meta::GenericValue& val_slot, const yaml::MiniYaml& yaml_trait) {
        val_slot = meta::GenericValue::Of(LoadOptionsImpl(yaml_trait));
      });
  meta::RegisterGeneratedLoader(
      std::format("{}.LoadFluentReferences", str_type_name),
      [](meta::GenericValue& val_slot, const yaml::MiniYaml& yaml_trait) {
        std::vector<meta::GenericValue> vec_ret;
        std::vector<std::string> vec_refs;
        for (const meta::GenericValue& val_option : LoadOptionsImpl(yaml_trait)) {
          const auto* rec_option =
              std::get_if<std::shared_ptr<meta::RecordObject>>(&val_option.val);
          if (rec_option == nullptr || *rec_option == nullptr)
            continue;
          if (const auto* rec_generated =
                  dynamic_cast<const meta::GeneratedRecord*>(rec_option->get()))
            AppendFluentReferences(*rec_generated, vec_refs);
        }
        for (const std::string& str_ref : vec_refs)
          vec_ret.push_back(meta::GenericValue::Of(str_ref));
        val_slot = meta::GenericValue::Of(std::move(vec_ret));
      });
}

}

void RegisterGameLoaders() {

  RegisterLoadSpeeds("LocomotorInfo.LoadSpeeds");
  RegisterLoadSpeeds("SubterraneanLocomotorInfo.LoadSpeeds");
  RegisterLoadSpeeds("JumpjetLocomotorInfo.LoadSpeeds");

  RegisterLoadFootprint("BuildingInfo.LoadFootprint");
  RegisterLoadFootprint("EnergyWallInfo.LoadFootprint");
  RegisterLoadFootprint("D2kBuildingInfo.LoadFootprint");

  RegisterLoadResourceTypes("ResourceLayerInfo.LoadResourceTypes",
                            "OpenRA.Mods.Common.Traits.ResourceLayerInfo+ResourceTypeInfo");

  RegisterLoadResourceTypes("EditorResourceLayerInfo.LoadResourceTypes",
                            "OpenRA.Mods.Common.Traits.ResourceLayerInfo+ResourceTypeInfo");
  RegisterLoadResourceTypes("ResourceRendererInfo.LoadResourceTypes",
                            "OpenRA.Mods.Common.Traits.ResourceRendererInfo+ResourceTypeInfo");

  meta::RegisterGeneratedLoader(
      "SupportPowerDecision.LoadConsiderations",
      [](meta::GenericValue& val_slot, const yaml::MiniYaml& yaml_trait) {
        std::vector<meta::GenericValue> vec_ret;
        for (const yaml::MiniYamlNode& node : yaml_trait.Nodes) {
          const std::string_view sv_key = KeyOf(node);
          const std::size_t int4_at = sv_key.find('@');
          const std::string_view sv_head =
              int4_at == std::string_view::npos ? sv_key : sv_key.substr(0, int4_at);
          if (sv_head != "Consideration")
            continue;
          vec_ret.push_back(meta::GenericValue::Of(MakeLoadedRecord(
              "OpenRA.Mods.Common.Traits.SupportPowerDecision+Consideration",
              node.Value)));
        }
        val_slot = meta::GenericValue::Of(std::move(vec_ret));
      });

  RegisterLoadInitialSmudges("SmudgeLayerInfo.LoadInitialSmudges");
  RegisterLoadShape("HitShapeInfo.LoadShape");
  RegisterLoadDecisions("SupportPowerBotModuleInfo.LoadDecisions");

  meta::RegisterGeneratedLoader(
      "MapGeneratorMultiChoiceOption.LoadChoices",
      [](meta::GenericValue& val_slot, const yaml::MiniYaml& yaml_trait) {
        meta::GenericDict map_ret;
        for (const yaml::MiniYamlNode& node : yaml_trait.Nodes) {
          const std::string_view sv_key = KeyOf(node);
          const std::size_t int4_at = sv_key.find('@');
          if (int4_at == std::string_view::npos || sv_key.substr(0, int4_at) != "Choice")
            continue;
          map_ret.emplace_back(
              meta::GenericValue::Of(std::string{sv_key.substr(int4_at + 1)}),
              meta::GenericValue::Of(MakeLoadedRecord(
                  "OpenRA.Mods.Common.MapGenerator.MapGeneratorMultiChoiceOption+MapGeneratorDropdownChoice",
                  node.Value)));
        }
        val_slot = meta::GenericValue::Of(std::move(map_ret));
      });

  meta::RegisterGeneratedLoader(
      "MapGeneratorDropdownChoice.LoadParameters",
      [](meta::GenericValue& val_slot, const yaml::MiniYaml& yaml_trait) {
        if (const yaml::MiniYamlNode* node_params =
                yaml_trait.NodeWithKeyOrDefault("Parameters")) {
          val_slot = meta::GenericValue::Of(
              yaml::WriteToString(node_params->Value.Nodes));
        } else {
          val_slot = meta::GenericValue::Of(std::string{});
        }
      });

  RegisterMapGeneratorLoaders("MapGeneratorBaseInfo");

  meta::RegisterLoader(
      "OpenRA.GameRules.WeaponInfo.LoadProjectile",
      [](void* p_field, const yaml::MiniYaml& yaml_trait) {
        auto& rec_target = *reinterpret_cast<std::shared_ptr<meta::RecordObject>*>(p_field);
        const yaml::MiniYamlNode* node_proj =
            yaml_trait.NodeWithKeyOrDefault("Projectile");
        if (node_proj == nullptr)
          return;
        const std::string str_name{ValueTextOf(node_proj->Value)};
        auto rec = meta::TypeRegistry::CreateObject(std::format("{}Info", str_name));
        if (rec == nullptr)
          throw yaml::YamlException(
              std::format("Cannot locate type: {}Info", str_name));
        meta::Load(rec.get(), node_proj->Value);
        rec_target = std::shared_ptr<meta::RecordObject>(std::move(rec));
      });
  meta::RegisterLoader(
      "OpenRA.GameRules.WeaponInfo.LoadWarheads",
      [](void* p_field, const yaml::MiniYaml& yaml_trait) {
        auto& vec_target =
            *reinterpret_cast<std::vector<std::shared_ptr<meta::RecordObject>>*>(p_field);
        vec_target.clear();
        for (const yaml::MiniYamlNode& node : yaml_trait.Nodes) {
          const std::string_view sv_key = KeyOf(node);
          if (!sv_key.starts_with("Warhead"))
            continue;
          const std::string str_name{ValueTextOf(node.Value)};
          auto rec = meta::TypeRegistry::CreateObject(std::format("{}Warhead", str_name));
          if (rec == nullptr) {

            continue;
          }
          meta::Load(rec.get(), node.Value);
          vec_target.push_back(std::shared_ptr<meta::RecordObject>(std::move(rec)));
        }
      });
}

}
