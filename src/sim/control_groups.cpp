// UPSTREAM: OpenRA.Mods.Common/Traits/World/ControlGroups.cs @b6fc03f
//          (实现部分)
//          The implementation half of ControlGroups.cs.
import std;

#include "sim/control_groups.hpp"

#include "meta/field_loader.hpp"
#include "meta/parse.hpp"
#include "sim/actor.hpp"
#include "sim/selection.hpp"
#include "sim/world.hpp"

namespace ora::sim {

namespace {

std::uint32_t ParseUIntInvariant(std::string_view sv) {
  // uint.Parse(NumberStyles.Integer):两侧空白允许,禁止负号(与
  // Selection 侧同一形态)| the same shape as Selection's helper.
  while (!sv.empty() &&
         (sv.front() == ' ' || sv.front() == '\t' || sv.front() == '\n' ||
          sv.front() == '\r'))
    sv.remove_prefix(1);
  while (!sv.empty() &&
         (sv.back() == ' ' || sv.back() == '\t' || sv.back() == '\n' ||
          sv.back() == '\r'))
    sv.remove_suffix(1);
  if (sv.empty() || sv.front() == '-')
    throw std::invalid_argument(
        "The input string '' was not in a correct format.");
  std::uint64_t v = 0;
  for (const char c : sv) {
    if (c < '0' || c > '9')
      throw std::invalid_argument(
          "The input string '' was not in a correct format.");
    v = v * 10 + static_cast<std::uint64_t>(c - '0');
    if (v > 0xFFFFFFFFull)
      throw std::overflow_error("Value was either too large or too small for a UInt32.");
  }
  return static_cast<std::uint32_t>(v);
}

}  // namespace

ControlGroups::ControlGroups(World& world, std::vector<std::string> groups)
    : world_{world}, groups_{std::move(groups)} {
  // Enumerable.Range(0, Groups.Length).Select(_ => new List<Actor>())
  // (L40)| (L40).
  vec_control_groups_.resize(groups_.size());
}

void ControlGroups::SelectControlGroup(int group) {
  // world.Selection.Combine(world, GetActorsInControlGroup(group), false,
  // false)(L45)| (L45).
  const std::vector<Actor*> group_actors = GetActorsInControlGroup(group);
  if (ISelection* selection = world_.Selection())
    selection->Combine(world_, group_actors, false, false);
}

void ControlGroups::CreateControlGroup(int group) {
  ISelection* selection = world_.Selection();
  if (selection == nullptr || selection->Actors().empty())  // L50
    return;

  vec_control_groups_[static_cast<std::size_t>(group)].clear();  // L53

  RemoveActorsFromAllControlGroups(selection->Actors());  // L55

  for (Actor* a : selection->Actors())  // L57
    if (a->Owner() == world_.LocalPlayer())
      vec_control_groups_[static_cast<std::size_t>(group)].push_back(a);
}

void ControlGroups::AddSelectionToControlGroup(int group) {
  ISelection* selection = world_.Selection();
  if (selection == nullptr || selection->Actors().empty())  // L62
    return;

  RemoveActorsFromAllControlGroups(selection->Actors());  // L65

  for (Actor* a : selection->Actors())  // L67
    if (a->Owner() == world_.LocalPlayer())
      vec_control_groups_[static_cast<std::size_t>(group)].push_back(a);
}

void ControlGroups::CombineSelectionWithControlGroup(int group) {
  const std::vector<Actor*> group_actors = GetActorsInControlGroup(group);
  if (ISelection* selection = world_.Selection())
    selection->Combine(world_, group_actors, true, false);
}

void ControlGroups::AddToControlGroup(Actor* a, int group) {
  auto& cg = vec_control_groups_[static_cast<std::size_t>(group)];
  // Contains 线性 + Add(L76-79)| the linear Contains + Add (L76-79).
  const std::uint32_t id = a->ActorID();
  const bool present = std::any_of(cg.begin(), cg.end(), [&](Actor* x) {
    return x->ActorID() == id;
  });
  if (!present)
    cg.push_back(a);
}

void ControlGroups::RemoveFromControlGroup(Actor* a) {
  const std::optional<int> group = GetControlGroupForActor(a);
  if (group.has_value()) {
    auto& cg = vec_control_groups_[static_cast<std::size_t>(*group)];
    const std::uint32_t id = a->ActorID();
    cg.erase(std::remove_if(cg.begin(), cg.end(),
                            [&](Actor* x) { return x->ActorID() == id; }),
             cg.end());
  }
}

std::optional<int> ControlGroups::GetControlGroupForActor(Actor* a) {
  const std::uint32_t id = a->ActorID();
  for (std::size_t i = 0; i < vec_control_groups_.size(); i++) {
    const auto& cg = vec_control_groups_[i];
    if (std::any_of(cg.begin(), cg.end(),
                    [&](Actor* x) { return x->ActorID() == id; }))
      return static_cast<int>(i);
  }
  return std::nullopt;
}

void ControlGroups::RemoveActorsFromAllControlGroups(
    std::span<Actor* const> actors) {
  for (std::size_t i = 0; i < groups_.size(); i++) {
    auto& cg = vec_control_groups_[i];
    cg.erase(
        std::remove_if(cg.begin(), cg.end(),
                       [&](Actor* a) {
                         const std::uint32_t id = a->ActorID();
                         return std::any_of(actors.begin(), actors.end(),
                                            [&](Actor* x) {
                                              return x->ActorID() == id;
                                            });
                       }),
        cg.end());
  }
}

std::vector<Actor*> ControlGroups::GetActorsInControlGroup(int group) {
  // Where(a => a.IsInWorld)(L105)| (L105).
  std::vector<Actor*> out;
  for (Actor* a : vec_control_groups_[static_cast<std::size_t>(group)])
    if (a->IsInWorld())
      out.push_back(a);
  return out;
}

void ControlGroups::Tick(Actor&) {
  for (auto& cg : vec_control_groups_) {
    // note: NOT `!a.IsInWorld`, since that would remove things that are in
    // transports.(上游注释)| (upstream comment).
    cg.erase(std::remove_if(cg.begin(), cg.end(),
                            [&](Actor* a) {
                              return a->Disposed() ||
                                     a->Owner() != world_.LocalPlayer();
                            }),
             cg.end());
  }
}

std::vector<yaml::MiniYamlNode> ControlGroups::IssueTraitData(Actor&) {
  // L117-134:FieldSaver.FormatValue(actorIds) 的逗号列表形态(与
  // Selection::IssueTraitData 同约定)
  // L117-134: the comma-list shape of FieldSaver.FormatValue(actorIds)
  // (the same convention as Selection::IssueTraitData).
  yaml::StringPool& pool = yaml::MiniYaml::GlobalPool();
  std::vector<yaml::MiniYamlNode> groups;
  for (std::size_t i = 0; i < vec_control_groups_.size(); i++) {
    const auto& cg = vec_control_groups_[i];
    if (cg.empty())
      continue;
    std::string str_ids;
    for (std::size_t j = 0; j < cg.size(); j++) {
      if (j != 0)
        str_ids += ", ";
      str_ids += std::format("{}", cg[j]->ActorID());
    }
    groups.emplace_back(pool.Intern(std::to_string(i)),
                        yaml::MiniYaml{pool.Intern(str_ids)});
  }

  yaml::MiniYamlNode root{
      pool.Intern("Groups"), yaml::MiniYaml{pool.Intern(std::string{}),
                                            std::move(groups)}};
  return {root};
}

void ControlGroups::ResolveTraitData(Actor& self, const yaml::MiniYaml& data) {
  // L136-148
  const yaml::MiniYamlNode* groups_node = data.NodeWithKeyOrDefault("Groups");
  if (groups_node == nullptr)
    return;

  for (const yaml::MiniYamlNode& n : groups_node->Value.Nodes) {
    const std::string_view sv_value =
        n.Value.Value != nullptr ? std::string_view{*n.Value.Value}
                                 : std::string_view{};
    // FieldLoader.GetValue<uint[]> 的逗号列表解析(失败段 = uint.Parse 抛)
    // The comma-list parse of FieldLoader.GetValue<uint[]> (a failing
    // segment = the uint.Parse throw).
    const std::string_view sv_key = n.Key != nullptr ? std::string_view{*n.Key}
                                                     : std::string_view{};
    const int group = meta::GetInt32Value("group", sv_key);
    for (const std::string_view part : meta::SplitComma(sv_value)) {
      const std::uint32_t id = ParseUIntInvariant(part);
      if (Actor* a = self.world().GetActorById(id))
        vec_control_groups_[static_cast<std::size_t>(group)].push_back(a);
    }
  }
}

}  // namespace ora::sim
