// UPSTREAM: OpenRA.Game/FieldLoader.cs @7d57605(24 个 [FieldLoader.LoadUsing]
//          加载器的逐语义移植,schema_dumper --scan 清单;各 loader 的源文件
//          行号在对应函数注记):
//          The verbatim-semantics port of all 24 [FieldLoader.LoadUsing]
//          loaders (the schema_dumper --scan list):
//  - Locomotor.cs LoadSpeeds ×4(Locomotor/Subterranean/Jumpjet;10000/speed
//    整除、PathingCost 覆盖、speed≤0 跳过)
//  - Building.cs LoadFootprint ×3(Building/EnergyWall/D2kBuilding)
//  - ResourceLayer.cs LoadResourceTypes ×8(各 ResourceTypeInfo 记录)
//  - SmudgeLayer.cs LoadInitialSmudges(逐项 try-catch 吞错)
//  - HitShape.cs LoadShape(缺省 CircleShape;异常包 "HitShape {shape}: ")
//  - SupportPowerBotModule.cs LoadDecisions
//  - MapGeneratorBase.cs LoadOptions/LoadFluentReferences ×10(4 类选项;
//    Fluent = Label + MultiChoice 的 Choice Labels)
//  - WeaponInfo.cs LoadProjectile/LoadWarheads(手写类路径,void* 注册表)
//
// 值袋承载 / Value-bag carriers:
//  - TerrainInfo/(ResourceTypeInfo/SupportPowerDecision/IHitShape/Option)等
//    嵌套记录 → GenericValue 的 rec 槽(GeneratedRecord + gen/ 描述表)
//  - 元组型载荷(TerrainInfo.Cost/Speed、MapSmudge.Type/Depth)→ GenericTuple /
//    字符串+int 混合 → 专用槽序(见各 loader 注释;dump 协议同步约定)
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

/// 便捷:构造一条 GeneratedRecord(按全名查描述表;未注册 = 生成器缺口,abort)
/// Convenience: build a GeneratedRecord (descriptor lookup by full name; an
/// unregistered name is a generator gap and aborts).
std::shared_ptr<meta::GeneratedRecord> MakeRecord(std::string_view str_full_name) {
  // 全名键双重寻址(嵌套同名类如两族 ResourceTypeInfo:简单名键会被
  // FullName 序的后者覆盖,工厂必须走全名)
  // Full-name dual addressing (nested same-named classes like the two
  // ResourceTypeInfo families: the simple-name key is overwritten by the
  // FullName-ordered later entry, so factories must go by full name).
  const meta::RecordDesc* desc = meta::TypeRegistry::FindType(str_full_name);
  auto rec = meta::TypeRegistry::CreateObject(str_full_name);
  if (desc == nullptr || rec == nullptr)
    throw std::runtime_error(std::format("MakeRecord: type not registered: {}", str_full_name));
  return std::shared_ptr<meta::GeneratedRecord>(
      static_cast<meta::GeneratedRecord*>(rec.release()));
}

/// 默认值表尺寸(与 CollectFields 展开)对齐检查后返回记录
/// Return the record with its defaults verified against CollectFields.
std::shared_ptr<meta::GeneratedRecord> MakeLoadedRecord(std::string_view str_full_name,
                                                        const yaml::MiniYaml& yaml_node) {
  auto rec = MakeRecord(str_full_name);
  meta::Load(rec.get(), yaml_node);
  return rec;
}

/// 节点键视图 / node-key view.
std::string_view KeyOf(const yaml::MiniYamlNode& node) {
  return node.Key != nullptr ? std::string_view{*node.Key} : std::string_view{};
}

/// 节点值文本视图 / node-value text view.
std::string_view ValueTextOf(const yaml::MiniYaml& yaml_v) {
  return yaml_v.Value != nullptr ? std::string_view{*yaml_v.Value} : std::string_view{};
}

// ———— LoadSpeeds(Locomotor.cs)/LoadSpeeds (Locomotor.cs) ————

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

          // pathingCost 覆盖或 10000/speed(int 整除,-fwrapv 下与 C# unchecked 一致)
          // The PathingCost override or 10000/speed (integer division,
          // wrapping identically to C# unchecked under -fwrapv).
          std::int16_t int2_cost{};
          if (const yaml::MiniYamlNode* node_cost =
                  node_terrain.Value.NodeWithKeyOrDefault("PathingCost")) {
            int2_cost = static_cast<std::int16_t>(
                meta::GetInt32Value("cost", ValueTextOf(node_cost->Value)));
          } else {
            int2_cost = static_cast<std::int16_t>(10000 / int4_speed);
          }

          // TerrainInfo(speed, cost)→ 元组槽(约定:arr_ints[0]=speed,[1]=cost)
          // TerrainInfo(speed, cost) → tuple slot (convention:
          // arr_ints[0]=speed, [1]=cost).
          map_ret.emplace_back(
              meta::GenericValue::Of(std::string{KeyOf(node_terrain)}),
              meta::GenericValue::Of(meta::GenericTuple{
                  .arr_ints = {int4_speed, int2_cost, 0, 0}, .arr_floats = {},
                  .uint1_count = 2}));
        }
        val_slot = meta::GenericValue::Of(std::move(map_ret));
      });
}

// ———— LoadFootprint(Building.cs)————

/// FootprintCellType 枚举(Building.cs L22-29):char 码点即值
/// The FootprintCellType enum (Building.cs L22-29): the char code is the
/// value.
constexpr bool IsFootprintCellDefined(std::int32_t int4_code) {
  return int4_code == '_' || int4_code == '=' || int4_code == 'x' ||
         int4_code == 'X' || int4_code == '+';
}

void RegisterLoadFootprint(std::string_view str_composite) {
  meta::RegisterGeneratedLoader(
      str_composite,
      [](meta::GenericValue& val_slot, const yaml::MiniYaml& yaml_trait) {
        // 缺省 footprint = ['x'](L155)
        // The default footprint = ['x'] (L155).
        std::vector<std::int32_t> vec_chars{'x'};
        if (const yaml::MiniYamlNode* node_fp =
                yaml_trait.NodeWithKeyOrDefault("Footprint")) {
          vec_chars.clear();
          for (const char ch : ValueTextOf(node_fp->Value))
            if (!std::isspace(static_cast<unsigned char>(ch)))
              vec_chars.push_back(static_cast<std::int32_t>(ch) & 0xFF);  // char 码点(char code point)
        }

        // 缺省 dimensions = (1,1)(L158)
        // The default dimensions = (1,1) (L158).
        std::int32_t int4_dimX{1}, int4_dimY{1};
        if (const yaml::MiniYamlNode* node_dims =
                yaml_trait.NodeWithKeyOrDefault("Dimensions")) {
          const std::vector<std::string_view> vec_parts =
              meta::SplitCommaTrimmed(ValueTextOf(node_dims->Value));
          if (vec_parts.size() == 2 &&
              meta::TryParseInt32Invariant(vec_parts[0], int4_dimX) &&
              meta::TryParseInt32Invariant(vec_parts[1], int4_dimY)) {
            // ok / ok
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

// ———— LoadResourceTypes(ResourceLayer.cs 等同族)————

/// 同族结构:ResourceTypes 节点 → {键 → <记录全名> 实例}
/// One shape for the family: the ResourceTypes node → {key → instances of
/// <record full name>}.
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

// ———— LoadInitialSmudges(SmudgeLayer.cs)————

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
              // MapSmudge { Type, Depth } → 键 → 混合槽(约定:
              // dict 值 = GenericDict{{"Type",str},{"Depth",int}} 的退化 —— 直接
              // 用元组不可承载字符串,采用 rec 槽不可行(结构体非生成集核心),
              // 以 dict{Type→str, Depth→int} 承载)
              // MapSmudge { Type, Depth } → a dict{Type→str, Depth→int} slot
              // (tuples cannot carry strings and the struct is outside the
              // generated core).
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
              // 上游逐项 catch{} 吞错(L93-107)
              // Upstream swallows per-item failures (catch{}, L93-107).
            }
          }
        }
        val_slot = meta::GenericValue::Of(std::move(map_ret));
      });
}

// ———— LoadShape(HitShape.cs)————

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
          // 缺省 CircleShape(L154-155)
          // The CircleShape default (L154-155).
          val_slot = meta::GenericValue::Of(MakeRecord("OpenRA.Mods.Common.HitShapes.CircleShape"));
        }
        // ret.Initialize()(L157):形状参数缓存(半径平方等运行时值,非 yaml 面;
        // dump 不含 —— 登记偏离,Phase 5 随行为面落地)
        // ret.Initialize() (L157): runtime shape caches (squared radii and
        // friends, outside the yaml surface; excluded from dumps — a
        // registered deviation landing with the Phase 5 behaviour surface).
      });
}

// ———— LoadDecisions(SupportPowerBotModule.cs)————

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

// ———— LoadOptions / LoadFluentReferences(MapGeneratorBase.cs)————

/// LoadOptions(L25-46):Options 节点 → "Kind@id" 键 → 4 类选项
/// LoadOptions (L25-46): the Options node → "Kind@id" keys → the four
/// option kinds.
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
      continue;  // Split('@') 长度 ≠ 2 → 跳过(L33-34)
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

    // 构造 (id, yaml):Id 为 [FieldLoader.Ignore](不进描述表/dump);
    // FieldLoader.Load(this, yaml) 即完整语义
    // The (id, yaml) construction: Id is [FieldLoader.Ignore] (absent from
    // descriptors/dumps); FieldLoader.Load(this, yaml) is the whole story.
    vec_ret.push_back(
        meta::GenericValue::Of(MakeLoadedRecord(sv_record, node.Value)));
  }
  return vec_ret;
}

/// GetFluentReferences:基类 Label;MultiChoice 覆写追加 Choices Labels
/// GetFluentReferences: the base Label; the MultiChoice override appends the
/// Choice Labels.
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

  // MultiChoice 的覆写(L154-165):Choices 值(LoadUsing 字典)的 Label
  // The MultiChoice override (L154-165): the Labels of the Choices values
  // (a LoadUsing dictionary).
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

}  // namespace

// ———— 总注册入口 / the aggregate registration entry ————

void RegisterGameLoaders() {
  // LoadSpeeds ×4(Locomotor/Subterranean/Jumpjet;同 TerrainInfo 结构)
  // LoadSpeeds ×4 (same TerrainInfo shape).
  RegisterLoadSpeeds("LocomotorInfo.LoadSpeeds");
  RegisterLoadSpeeds("SubterraneanLocomotorInfo.LoadSpeeds");
  RegisterLoadSpeeds("JumpjetLocomotorInfo.LoadSpeeds");

  // LoadFootprint ×3
  RegisterLoadFootprint("BuildingInfo.LoadFootprint");
  RegisterLoadFootprint("EnergyWallInfo.LoadFootprint");
  RegisterLoadFootprint("D2kBuildingInfo.LoadFootprint");

  // LoadResourceTypes:三个自有 loader(ResourceLayer/EditorResourceLayer/
  // ResourceRenderer);TS/D2k 各 Info 全部继承基类 loader,无自有注册
  // LoadResourceTypes: three own loaders (ResourceLayer/EditorResourceLayer/
  // ResourceRenderer); the TS/D2k Infos all inherit the base loaders.
  RegisterLoadResourceTypes("ResourceLayerInfo.LoadResourceTypes",
                            "OpenRA.Mods.Common.Traits.ResourceLayerInfo+ResourceTypeInfo");
  // EditorResourceLayer.cs L28-38("Copied from ResourceLayerInfo"):
  // 构造的是 ResourceLayerInfo.ResourceTypeInfo
  // EditorResourceLayer.cs L28-38 ("Copied from ResourceLayerInfo"):
  // constructs ResourceLayerInfo.ResourceTypeInfo.
  RegisterLoadResourceTypes("EditorResourceLayerInfo.LoadResourceTypes",
                            "OpenRA.Mods.Common.Traits.ResourceLayerInfo+ResourceTypeInfo");
  RegisterLoadResourceTypes("ResourceRendererInfo.LoadResourceTypes",
                            "OpenRA.Mods.Common.Traits.ResourceRendererInfo+ResourceTypeInfo");

  // SupportPowerDecision.LoadConsiderations(SupportPowerDecision.cs L62-69:
  // "Consideration@…" 节点 → Consideration 记录数组)
  // SupportPowerDecision.LoadConsiderations (SupportPowerDecision.cs L62-69:
  // the "Consideration@…" nodes → an array of Consideration records).
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

  // LoadInitialSmudges / LoadShape / LoadDecisions
  RegisterLoadInitialSmudges("SmudgeLayerInfo.LoadInitialSmudges");
  RegisterLoadShape("HitShapeInfo.LoadShape");
  RegisterLoadDecisions("SupportPowerBotModuleInfo.LoadDecisions");

  // MapGeneratorMultiChoiceOption.LoadChoices(MapGeneratorOptions.cs L109-121:
  // "Choice@id" 键 → MapGeneratorDropdownChoice 记录字典)
  // MapGeneratorMultiChoiceOption.LoadChoices (MapGeneratorOptions.cs L109-121:
  // the "Choice@id" keys → the MapGeneratorDropdownChoice record dict).
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

  // MapGeneratorDropdownChoice.LoadParameters(L95-102:Parameters 节点数组;
  // 值袋以 WriteToString 规范化文本承载 —— dump 协议两侧同源)
  // MapGeneratorDropdownChoice.LoadParameters (L95-102: the Parameters node
  // array; carried in the bag as WriteToString-normalized text — the dump
  // protocol derives both sides from the same source).
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

  // MapGenerator 族(声明类 MapGeneratorBaseInfo:Classic/TS/D2k/Clear 均继承,
  // 复合名经 FlattenHierarchy 归一到声明类)
  // The MapGenerator family (declaring class MapGeneratorBaseInfo: Classic/
  // TS/D2k/Clear all inherit; composite names normalize to the declaring
  // class via FlattenHierarchy).
  RegisterMapGeneratorLoaders("MapGeneratorBaseInfo");

  // ———— WeaponInfo 的两个手写类 loader(void* 注册表;WeaponInfo.cs L147-175)————
  // The two hand-written WeaponInfo loaders (the void* registry; WeaponInfo.cs
  // L147-175).
  meta::RegisterLoader(
      "OpenRA.GameRules.WeaponInfo.LoadProjectile",
      [](void* p_field, const yaml::MiniYaml& yaml_trait) {
        auto& rec_target = *reinterpret_cast<std::shared_ptr<meta::RecordObject>*>(p_field);
        const yaml::MiniYamlNode* node_proj =
            yaml_trait.NodeWithKeyOrDefault("Projectile");
        if (node_proj == nullptr)
          return;  // proj == null → null(L150-152)
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
            // 上游 ret==null → continue(linter 路径;运行时数据不触发)
            // Upstream ret==null → continue (the linter path; runtime data
            // never triggers it).
            continue;
          }
          meta::Load(rec.get(), node.Value);
          vec_target.push_back(std::shared_ptr<meta::RecordObject>(std::move(rec)));
        }
      });
}

}  // namespace ora::game
