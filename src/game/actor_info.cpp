// UPSTREAM: OpenRA.Game/GameRules/ActorInfo.cs @b6fc03f(actor_info.hpp
//          的实现;LoadTraitInfo/拓扑序逐语义,见 hpp 头注)
//          Implementation of actor_info.hpp — LoadTraitInfo and the
//          topological order verbatim; see the hpp header notes.
import std;
#include "game/actor_info.hpp"
#include "meta/field_loader.hpp"

namespace ora::game {

namespace {

/// 类型是否实现给定接口(RecordDesc::interfaces 含基类链接口)
/// Whether the type implements the given interface (RecordDesc::interfaces
/// includes the base-chain interfaces).
bool ImplementsInterface(const meta::RecordDesc& desc, std::string_view str_interface) {
  for (const std::string_view sv_i : desc.interfaces)
    if (sv_i == str_interface)
      return true;
  // 泛型接口闭式比较:IRulesetLoaded`1[[OpenRA.ActorInfo,…]] 这类带程序集
  // 限定的全名按 "`1[[OpenRA.ActorInfo" 前缀匹配 IRulesetLoaded<ActorInfo>
  // Generic-interface closed match: assembly-qualified names like
  // IRulesetLoaded`1[[OpenRA.ActorInfo,…]] match IRulesetLoaded<ActorInfo>
  // by the "`1[[<arg-namespace>." prefix.
  const std::size_t int4_gen = str_interface.find('<');
  if (int4_gen == std::string_view::npos)
    return false;
  const std::string_view sv_open = str_interface.substr(0, int4_gen);      // "IRulesetLoaded"
  const std::string_view sv_arg = str_interface.substr(int4_gen + 1);      // "ActorInfo>"
  const std::string_view sv_argName = sv_arg.substr(0, sv_arg.find_first_of(">,"));  // "ActorInfo"
  for (const std::string_view sv_i : desc.interfaces) {
    const std::size_t int4_tick = sv_i.find('`');
    if (int4_tick == std::string_view::npos || sv_i.substr(0, int4_tick) != sv_open)
      continue;
    const std::size_t int4_bracket = sv_i.find("[[");
    if (int4_bracket == std::string_view::npos)
      continue;
    // 实参全名取 "...[[OpenRA.Mods….AttackBaseInfo," → 取末段类型名比较
    // The argument full name "...[[OpenRA.Mods….AttackBaseInfo," — compare
    // the trailing type name.
    const std::string_view sv_full = sv_i.substr(int4_bracket + 2);
    const std::size_t int4_comma = sv_full.find(',');
    const std::string_view sv_typeName =
        sv_full.substr(0, int4_comma == std::string_view::npos ? sv_full.size() : int4_comma);
    const std::size_t int4_lastDot = sv_typeName.rfind('.');
    const std::string_view sv_simple =
        sv_typeName.substr(int4_lastDot == std::string_view::npos ? 0 : int4_lastDot + 1);
    if (sv_simple == sv_argName)
      return true;
  }
  return false;
}

}  // namespace

ActorInfo::ActorInfo(std::string str_name, const yaml::MiniYaml& node)
    : str_name_{std::move(str_name)} {
  try {
    for (const yaml::MiniYamlNode& node_trait : node.Nodes) {
      try {
        // HACK 注记(上游 L48-50):linter 场景 trait 缺失时只报错不炸;
        // C++ 侧未知 trait 抛 YamlException(非 linter 行为,与上游
        // InvalidOperationException 的运行时路径消息不同 —— 登记偏离)
        // HACK note (upstream L48-50): the linter only reports missing
        // traits; the C++ side raises YamlException for unknown traits (the
        // non-linter path; the message differs from the upstream
        // InvalidOperationException — registered deviation).
        const std::string_view sv_traitName =
            node_trait.Key != nullptr ? std::string_view{*node_trait.Key}
                                      : std::string_view{};
        std::string str_instanceName;
        auto rec_trait = LoadTraitInfo(sv_traitName, node_trait.Value, str_instanceName);
        if (rec_trait != nullptr) {
          vec_traits_.push_back(std::move(rec_trait));
          vec_instanceNames_.push_back(std::move(str_instanceName));
        }
      } catch (const meta::MissingFieldsException& e_except) {
        throw yaml::YamlException(e_except.what());
      }
    }
  } catch (const yaml::YamlException& e_except) {
    // L62-65:统一包上 actor 名
    // L62-65: uniformly rewrap with the actor name.
    throw yaml::YamlException(
        std::format("Actor type {}: {}", str_name_, e_except.what()));
  }
}

ActorInfo::ActorInfo(std::string str_name) : str_name_{std::move(str_name)} {}

std::unique_ptr<meta::RecordObject> ActorInfo::LoadTraitInfo(
    std::string_view sv_trait_name, const yaml::MiniYaml& my,
    std::string& str_instance_name_out) {
  // L78-79:junk 值检查
  // L78-79: the junk-value check.
  if (my.Value != nullptr && !my.Value->empty())
    throw yaml::YamlException(
        std::format("Junk value `{}` on trait node {}", *my.Value, sv_trait_name));

  // L83-84:'@' 切分 + CreateObject<TraitInfo>(name + "Info")
  // L83-84: the '@' split + CreateObject<TraitInfo>(name + "Info").
  const std::size_t int4_at = sv_trait_name.find(kTraitInstanceSeparator);
  const std::string_view sv_type = int4_at == std::string_view::npos
                                       ? sv_trait_name
                                       : sv_trait_name.substr(0, int4_at);
  if (int4_at != std::string_view::npos)
    str_instance_name_out = std::string{sv_trait_name.substr(int4_at + 1)};

  auto rec_info = meta::TypeRegistry::CreateObject(std::format("{}Info", sv_type));
  if (rec_info == nullptr)
    throw yaml::YamlException(
        std::format("Cannot locate type: {}Info", sv_type));

  try {
    meta::Load(rec_info.get(), my);
  } catch (const meta::MissingFieldsException& e_except) {
    // L95-99:头部 = "Trait name {traitName}: " + 单/复数文案
    // L95-99: header = "Trait name {traitName}: " + the singular/plural text.
    const std::string str_header = std::format(
        "Trait name {}: {}",
        sv_trait_name,
        e_except.Missing().size() > 1 ? "Required properties missing"
                                      : "Required property missing");
    throw meta::MissingFieldsException(e_except.Missing(), str_header);
  }

  return rec_info;
}

std::vector<const meta::RecordObject*> ActorInfo::TraitInfosByInterface(
    std::string_view str_interface) const {
  std::vector<const meta::RecordObject*> vec_ret;
  for (const auto& rec_trait : vec_traits_)
    if (ImplementsInterface(rec_trait->record_desc(), str_interface))
      vec_ret.push_back(rec_trait.get());
  return vec_ret;
}

std::string_view ActorInfo::InstanceNameOf(
    const meta::RecordObject* rec) const {
  for (std::size_t i = 0; i < vec_traits_.size(); i++)
    if (vec_traits_[i].get() == rec)
      return vec_instanceNames_[i];
  return {};
}

bool ActorInfo::HasTraitInfoOfInterface(std::string_view str_interface) const {
  for (const auto& rec_trait : vec_traits_)
    if (ImplementsInterface(rec_trait->record_desc(), str_interface))
      return true;
  return false;
}

std::vector<const meta::RecordObject*> ActorInfo::TraitsInConstructOrder() const {
  // L109-167 逐语义:resolved = 无依赖起点;每轮把"全部依赖已满足"的未解析
  // 项移入 resolved,直到空;剩余 = 环/缺依赖 → 异常文本逐字
  // L109-167 verbatim: resolved starts with dependency-free entries; each
  // pass moves every unresolved entry whose dependencies are all satisfied
  // into resolved until none remain; leftovers (cycles/missing) raise with
  // the verbatim exception text.
  struct Entry {
    const meta::RecordObject* rec;
    const meta::RecordDesc* desc;
    std::vector<std::string_view> vec_deps;         // Requires(必依赖)
    std::vector<std::string_view> vec_optDeps;      // NotBefore(可选依赖)
  };

  const auto are_resolvable = [](std::string_view sv_dep, const meta::RecordDesc& desc_other) {
    // a.IsAssignableFrom(b):b 的类名链或实现接口集中出现 a(简单名比较;
    // 上游 Requires<T> 的 T 含接口形态,如 Requires<IHealthInfo> —— 接口
    // 全名的末段与依赖名比对)
    // a.IsAssignableFrom(b): a appears on b's class chain or implemented
    // interfaces (simple-name comparison; upstream Requires<T> arguments
    // include interface shapes like Requires<IHealthInfo> — matched by the
    // trailing segment of the interface full names).
    for (const meta::RecordDesc* desc_cur = &desc_other;;) {
      if (sv_dep == desc_cur->str_name)
        return true;
      for (const std::string_view sv_iface : desc_cur->interfaces) {
        const std::size_t int4_lastDot = sv_iface.rfind('.');
        const std::string_view sv_simple =
            sv_iface.substr(int4_lastDot == std::string_view::npos ? 0 : int4_lastDot + 1);
        const std::size_t int4_tick = sv_simple.find('`');
        if (sv_dep == (int4_tick == std::string_view::npos
                           ? sv_simple
                           : sv_simple.substr(0, int4_tick)))
          return true;
      }
      if (desc_cur->str_base.empty())
        return false;
      const meta::RecordDesc* desc_base = meta::TypeRegistry::FindType(desc_cur->str_base);
      if (desc_base == nullptr)
        return sv_dep == desc_cur->str_base;
      desc_cur = desc_base;
    }
  };

  std::vector<Entry> vec_source;
  for (const auto& rec_trait : vec_traits_) {
    const meta::RecordDesc& desc = rec_trait->record_desc();
    Entry entry{.rec = rec_trait.get(), .desc = &desc, .vec_deps = {}, .vec_optDeps = {}};
    for (const std::string_view sv_r : desc.requires_types) entry.vec_deps.push_back(sv_r);
    for (const std::string_view sv_n : desc.not_before_types) entry.vec_optDeps.push_back(sv_n);
    vec_source.push_back(std::move(entry));
  }

  const auto unresolved_pred = [&](const Entry& entry) {
    return entry.vec_deps.empty() && entry.vec_optDeps.empty();
  };
  std::vector<const Entry*> vec_resolved;
  std::vector<const Entry*> vec_unresolved;
  for (const Entry& entry : vec_source)
    (unresolved_pred(entry) ? vec_resolved : vec_unresolved).push_back(&entry);

  const auto more = [&]() {
    std::vector<const Entry*> vec_more;
    for (const Entry* entry : vec_unresolved) {
      const bool b_depsOk = std::all_of(
          entry->vec_deps.begin(), entry->vec_deps.end(), [&](std::string_view sv_d) {
            // 依赖满足:存在已解析项满足之,且没有任何未解析项也满足之
            // Satisfied: some resolved entry matches it and no unresolved
            // entry does.
            const bool b_resolvedMatch =
                std::any_of(vec_resolved.begin(), vec_resolved.end(),
                            [&](const Entry* r) { return are_resolvable(sv_d, *r->desc); });
            const bool b_unresolvedMatch =
                std::any_of(vec_unresolved.begin(), vec_unresolved.end(),
                            [&](const Entry* u) { return are_resolvable(sv_d, *u->desc); });
            return b_resolvedMatch && !b_unresolvedMatch;
          });
      const bool b_optOk = std::all_of(
          entry->vec_optDeps.begin(), entry->vec_optDeps.end(), [&](std::string_view sv_d) {
            const bool b_unresolvedMatch =
                std::any_of(vec_unresolved.begin(), vec_unresolved.end(),
                            [&](const Entry* u) { return are_resolvable(sv_d, *u->desc); });
            return !b_unresolvedMatch;
          });
      if (b_depsOk && b_optOk)
        vec_more.push_back(entry);
    }
    return vec_more;
  };

  std::vector<const Entry*> vec_ready = more();
  while (!vec_ready.empty()) {
    for (const Entry* entry : vec_ready)
      vec_resolved.push_back(entry);
    for (const Entry* entry : vec_ready)
      std::erase(vec_unresolved, entry);
    vec_ready = more();
  }

  if (!vec_unresolved.empty()) {
    // L146-162:异常文本逐字(Missing = 未被任何 source 满足的依赖;
    // Unresolved = 依赖未解析项集合)
    // L146-162: verbatim exception text (Missing = dependencies satisfied by
    // no source entry; Unresolved = the unresolved-entry dependency sets).
    std::string str_ex = std::format(
        "ActorInfo(\"{}\") failed to initialize because of the following:\n",
        str_name_);

    str_ex += "Missing:\n";
    for (const Entry* entry : vec_unresolved)
      for (const std::string_view sv_d : entry->vec_deps) {
        const bool b_any =
            std::any_of(vec_source.begin(), vec_source.end(), [&](const Entry& s) {
              return are_resolvable(sv_d, *s.desc);
            });
        if (!b_any)
          str_ex += std::format("{} \n", sv_d);
      }

    str_ex += "Unresolved:\n";
    for (const Entry* entry : vec_unresolved) {
      std::vector<std::string_view> vec_all;
      for (const std::string_view sv_d : entry->vec_deps) {
        const bool b_resolvedMatch =
            std::any_of(vec_resolved.begin(), vec_resolved.end(),
                        [&](const Entry* r) { return are_resolvable(sv_d, *r->desc); });
        if (!b_resolvedMatch)
          vec_all.push_back(sv_d);
      }
      std::string str_deps;
      bool b_first = true;
      for (const std::string_view sv_d : vec_all) {
        if (!b_first)
          str_deps += ", ";
        b_first = false;
        str_deps += std::format("{}", sv_d);
      }
      for (const std::string_view sv_d : entry->vec_optDeps) {
        const bool b_resolvedMatch =
            std::any_of(vec_resolved.begin(), vec_resolved.end(),
                        [&](const Entry* r) { return are_resolvable(sv_d, *r->desc); });
        if (!b_resolvedMatch) {
          if (!b_first)
            str_deps += ", ";
          b_first = false;
          str_deps += std::format("[{}]", sv_d);
        }
      }
      str_ex += std::format("{}: {{ {} }}\n", entry->desc->str_name, str_deps);
    }

    throw yaml::YamlException(str_ex);
  }

  std::vector<const meta::RecordObject*> vec_ret;
  vec_ret.reserve(vec_resolved.size());
  for (const Entry* entry : vec_resolved)
    vec_ret.push_back(entry->rec);
  return vec_ret;
}

}  // namespace ora::game
