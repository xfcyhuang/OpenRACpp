// UPSTREAM: OpenRA.Game/ObjectCreator.cs @b6fc03f L78-137(按名实例化)+
//          TraitInfo.Create(ActorInitializer) 的分发面(PORTING_PLAN D26/D27
//          的 Phase 5 接线)
//          The name-keyed instantiation of ObjectCreator + the dispatch face
//          of TraitInfo.Create — the D26/D27 Phase 5 wiring.
//
// 机制对照 / Mechanism mapping:
//  - 上游 traitInfo.Create(init) 是 Info 类的虚方法;C++ 侧 Info 走
//    meta::GeneratedRecord 值袋(无方法),故以"注册表 + 登记函数"承载:
//    Info 类型名 → 工厂(RecordObject&, ActorInitializer&, WorldArena&) →
//    TraitBase*(World arena 内构造 —— 所有权随 D26/D27 移入 arena)
//    Upstream's traitInfo.Create(init) is virtual on the Info class; the
//    C++ Info rides the GeneratedRecord value bag (method-free), so a
//    registry + a registration function carries it: the Info type name → a
//    factory (RecordObject&, ActorInitializer&, WorldArena&) → TraitBase*
//    (constructed inside the World arena — ownership moved there per
//    D26/D27).
//  - 未知名异常 = 上游 ObjectCreator.FindType null 的
//    "Cannot locate type: {name}" 文本逐字
//    An unknown name throws "Cannot locate type: {name}" verbatim.
//  - 生成类型无行为 → 值袋没有 Create 方法体;工厂即该 Info 的 Create 语义
//    (字段读取经按名槽位辅助)
//    Generated types carry no behaviour → the bag has no Create body; the
//    factory IS that Info's Create semantics (field reads through the
//    by-name slot helpers).
#pragma once
import std;

#include "core/arena.hpp"
#include "meta/generic_record.hpp"

namespace ora::game {
class ActorInfo;
}

namespace ora::sim {

class ActorInitializer;
class TraitBase;

/// trait 工厂协议(info 值袋 + init)→ arena 内构造的 trait 对象
/// The trait-factory protocol (the info bag + init) → the trait object
/// constructed inside the arena.
using TraitCreateFn = std::function<TraitBase*(
    const meta::RecordObject& rec_info, ActorInitializer& init,
    ora::WorldArena& arena)>;

/// TraitRegistry:Info 类型短名 → 工厂(ObjectCreator 等价)
/// TraitRegistry: the Info short-name → factory map (the ObjectCreator
/// equivalent).
class TraitRegistry {
 public:
  static TraitRegistry& Instance();

  void Register(std::string str_name, TraitCreateFn fn_factory);

  /// 未知名 = 未注册的 Info → nullptr(部分覆盖装配面;上游 ObjectCreator
  /// 全量注册的等效面随各 trait 批补齐 —— COVERAGE 登记。抛点保留于
  /// CreateOrThrow,供非装配路径锚定 "Cannot locate type: {name}" 逐字)
  /// An unknown name yields nullptr (the partial-coverage assembly face;
  /// upstream's fully-populated ObjectCreator arrives trait batch by trait
  /// batch — in COVERAGE). CreateOrThrow keeps the verbatim
  /// "Cannot locate type: {name}" throw for non-assembly paths.
  TraitBase* Create(const std::string& str_name,
                    const meta::RecordObject& rec_info,
                    ActorInitializer& init, ora::WorldArena& arena) const;
  [[noreturn]] static void ThrowCannotLocate(const std::string& str_name);

  bool Contains(std::string_view str_name) const;

 private:
  std::vector<std::pair<std::string, TraitCreateFn>> vec_entries_;
};

/// 世界系统 trait 注册(引擎与测试的装配入口各调用一次;与
/// RegisterGameLoaders 同形)
/// Registers the world-system traits (each assembly entry — engine or
/// tests — calls once; the same shape as RegisterGameLoaders).
void RegisterWorldTraits();

// ———— 生成记录的按名读值辅助(值袋槽位定位)————
// ———— The by-name value readers over a generated record (slot lookup) ————

/// 按接口全名过滤 Info(ActorInfo.TraitInfos<T> 的记录面;取首条)
/// The by-interface-name Info filter (the record face of
/// ActorInfo.TraitInfos<T>; first hit).
const meta::RecordObject* FindTraitInfoOfInterface(
    const game::ActorInfo& info, std::string_view str_interface);

/// 按名读 string 槽(缺字段 = nullopt) | reads a string slot by name
/// (nullopt when the field is absent).
std::optional<std::string_view> RecordFieldString(const meta::RecordObject& rec,
                                                  std::string_view str_name);

/// 按名读整型槽(缺字段 = nullopt) | reads an integer slot by name.
std::optional<std::int64_t> RecordFieldInt(const meta::RecordObject& rec,
                                           std::string_view str_name);

}  // namespace ora::sim
