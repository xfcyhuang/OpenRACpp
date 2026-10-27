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

}

ControlGroups::ControlGroups(World& world, std::vector<std::string> groups)
    : world_{world}, groups_{std::move(groups)} {

  vec_control_groups_.resize(groups_.size());
}

void ControlGroups::SelectControlGroup(int group) {

  const std::vector<Actor*> group_actors = GetActorsInControlGroup(group);
  if (ISelection* selection = world_.Selection())
    selection->Combine(world_, group_actors, false, false);
}

void ControlGroups::CreateControlGroup(int group) {
  ISelection* selection = world_.Selection();
  if (selection == nullptr || selection->Actors().empty())
    return;

  vec_control_groups_[static_cast<std::size_t>(group)].clear();

  RemoveActorsFromAllControlGroups(selection->Actors());

  for (Actor* a : selection->Actors())
    if (a->Owner() == world_.LocalPlayer())
      vec_control_groups_[static_cast<std::size_t>(group)].push_back(a);
}

void ControlGroups::AddSelectionToControlGroup(int group) {
  ISelection* selection = world_.Selection();
  if (selection == nullptr || selection->Actors().empty())
    return;

  RemoveActorsFromAllControlGroups(selection->Actors());

  for (Actor* a : selection->Actors())
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

  std::vector<Actor*> out;
  for (Actor* a : vec_control_groups_[static_cast<std::size_t>(group)])
    if (a->IsInWorld())
      out.push_back(a);
  return out;
}

void ControlGroups::Tick(Actor&) {
  for (auto& cg : vec_control_groups_) {

    cg.erase(std::remove_if(cg.begin(), cg.end(),
                            [&](Actor* a) {
                              return a->Disposed() ||
                                     a->Owner() != world_.LocalPlayer();
                            }),
             cg.end());
  }
}

std::vector<yaml::MiniYamlNode> ControlGroups::IssueTraitData(Actor&) {

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

  const yaml::MiniYamlNode* groups_node = data.NodeWithKeyOrDefault("Groups");
  if (groups_node == nullptr)
    return;

  for (const yaml::MiniYamlNode& n : groups_node->Value.Nodes) {
    const std::string_view sv_value =
        n.Value.Value != nullptr ? std::string_view{*n.Value.Value}
                                 : std::string_view{};

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

}
