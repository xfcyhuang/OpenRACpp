// UPSTREAM: OpenRA.Game/GameRules/ActorInfo.cs @7d57605 L18-201(逐语义重写)
//          + OpenRA.Game/Primitives/TypeDictionary.cs L30-141(本文件所需的
//          最小子集:接口名过滤查询)
//          Full rewrite of ActorInfo.cs + the minimal TypeDictionary subset
//          needed here (interface-name filtered queries).
//
// 机制对照 / Mechanism mapping:
//  - traits 存储:C# TypeDictionary(接口类型键 + List)→ 此处以
//    vector<unique_ptr<RecordObject>> + 接口名线性过滤(RecordDesc::
//    interfaces);Phase 3 的 TraitDictionary(接口 ID + 平行数组二分)落地时
//    替换存储
//    Trait storage: the C# TypeDictionary (interface-type keys + lists)
//    becomes a vector of Records plus linear interface-name filtering
//    (RecordDesc::interfaces); the Phase 3 TraitDictionary (interface IDs +
//    parallel-array bisection) replaces the storage when it lands.
//  - PrerequisitesOf/OptionalPrerequisitesOf(L169-185):反射扫 Requires<>/
//    NotBefore<> 泛型接口 → RecordDesc::requires_types/not_before_types
//    (schema_dumper 导出)
//    PrerequisitesOf/OptionalPrerequisitesOf (L169-185): the reflection scan
//    for the Requires<>/NotBefore<> generic interfaces → the exported
//    requires_types/not_before_types.
//  - InstanceName:C# 反射写字段([FieldLoader.Ignore]);值袋侧无该字段,
//    与 trait 并列存放(instance 视图;dump 输出用)
//    InstanceName: upstream writes it via reflection (a [FieldLoader.Ignore]
//    field); the value bag has no such field, so it sits alongside the trait
//    (an instance view; used by dump output).
#pragma once
import std;

#include "meta/generic_record.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::game {

/// ActorInfo(ActorInfo.cs L23):一个 actor 定义(name + trait 列表)
/// ActorInfo (ActorInfo.cs L23): one actor definition (name + trait list).
class ActorInfo final {
 public:
  static constexpr char kAbstractActorPrefix = '^';      // L25
  static constexpr char kTraitInstanceSeparator = '@';   // L26

  /// name 已小写(调用方 k.Key.ToLowerInvariant);node = actor 规则节点
  /// name is already lower-cased by the caller (k.Key.ToLowerInvariant);
  /// node = the actor rule node.
  ActorInfo(std::string str_name, const yaml::MiniYaml& node);

  /// 测试/系统 actor 构造(ActorInfo(name) —— 空 trait)
  /// Test/system-actor construction (ActorInfo(name) — no traits).
  explicit ActorInfo(std::string str_name);

  const std::string& Name() const { return str_name_; }

  /// 已加载 trait(yaml 声明序;InstanceName 为 '@' 后缀,无则空)
  /// The loaded traits (yaml declaration order; InstanceName is the '@'
  /// suffix, empty without one).
  const std::vector<std::unique_ptr<meta::RecordObject>>& Traits() const {
    return vec_traits_;
  }
  const std::vector<std::string>& TraitInstanceNames() const {
    return vec_instanceNames_;
  }

  /// TraitInfos<T>(TypeDictionary.WithInterface<T> 的接口名等价)
  /// TraitInfos<T> (the interface-name equivalent of
  /// TypeDictionary.WithInterface<T>).
  std::vector<const meta::RecordObject*> TraitInfosByInterface(
      std::string_view str_interface) const;

  /// HasTraitInfo<T>(L187)
  bool HasTraitInfoOfInterface(std::string_view str_interface) const;

  /// TraitsInConstructOrder(L104-167):Requires/NotBefore 拓扑序
  /// TraitsInConstructOrder (L104-167): the Requires/NotBefore topological
  /// order.
  std::vector<const meta::RecordObject*> TraitsInConstructOrder() const;

 private:
  /// LoadTraitInfo(L76-102):TraitName@Instance → 实例化 + FieldLoader.Load
  /// LoadTraitInfo (L76-102): TraitName@Instance → instantiate +
  /// FieldLoader.Load.
  static std::unique_ptr<meta::RecordObject> LoadTraitInfo(
      std::string_view sv_trait_name, const yaml::MiniYaml& my,
      std::string& str_instance_name_out);

  std::string str_name_;
  std::vector<std::unique_ptr<meta::RecordObject>> vec_traits_;
  std::vector<std::string> vec_instanceNames_;
};

}  // namespace ora::game
