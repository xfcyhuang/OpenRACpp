// UPSTREAM: OpenRA.Mods.Common/Traits/World/Selection.cs @b6fc03f
//          (selection.hpp 的实现) | The implementation of selection.hpp.
import std;

#include "sim/selection.hpp"

#include "game/actor_info.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::sim {

namespace {

/// Sync.RunUnsynced(world, …) 的等价入口(Sync.cs 的 true 缺省)
/// The Sync.RunUnsynced(world, …) equivalent (Sync.cs's implicit true).
void RunUnsyncedWorld(World* world, const std::function<void()>& fn) {
  RunUnsynced(true, world, fn);
}

std::uint32_t ParseUIntInvariant(std::string_view sv) {
  // uint.Parse(NumberStyles.Integer):两侧空白允许,禁止负号
  while (!sv.empty() &&
         (sv.front() == ' ' || sv.front() == '\t' || sv.front() == '\n' ||
          sv.front() == '\r'))
    sv.remove_prefix(1);
  while (!sv.empty() &&
         (sv.back() == ' ' || sv.back() == '\t' || sv.back() == '\n' ||
          sv.back() == '\r'))
    sv.remove_suffix(1);
  std::uint32_t v = 0;
  const auto [p, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), v);
  if (ec != std::errc{} || p != sv.data() + sv.size())
    throw std::runtime_error(std::format(
        "FieldLoader: Cannot parse `{}` into field `Selection` of type "
        "`UInt32[]`",
        sv));
  return v;
}

}  // namespace

void Selection::Created(Actor& self) {
  // L35-39
  vec_world_notify_selection_ = self.TraitsImplementing<INotifySelection>();
  ptr_world_ = &self.world();
}

void Selection::Add(Actor* a) {
  // L49-60
  if (std::find(vec_actors_.begin(), vec_actors_.end(), a) ==
      vec_actors_.end())
    vec_actors_.push_back(a);
  UpdateHash();

  for (auto* sel : a->TraitsImplementing<INotifySelected>())
    sel->Selected(*a);

  World& world = *ptr_world_;
  RunUnsyncedWorld(
      &world,
      [&] {
        if (world.OrderGenerator() != nullptr)
          world.OrderGenerator()->SelectionChanged(world, vec_actors_);
      });
  for (auto* ns : vec_world_notify_selection_)
    ns->SelectionChanged();
}

void Selection::Remove(Actor* a) {
  // L62-71
  if (std::erase(vec_actors_, a) == 0)
    return;
  UpdateHash();
  World& world = *ptr_world_;
  RunUnsyncedWorld(
      &world,
      [&] {
        if (world.OrderGenerator() != nullptr)
          world.OrderGenerator()->SelectionChanged(world, vec_actors_);
      });
  for (auto* ns : vec_world_notify_selection_)
    ns->SelectionChanged();
}

void Selection::OnOwnerChanged(Actor& a, Player& old_owner, Player&) {
  // L73-84
  if (!Contains(&a))
    return;

  // Remove the actor from the original owners selection(上游注释)
  if (&old_owner == ptr_world_->LocalPlayer())
    Remove(&a);
  else
    UpdateHash();
}

bool Selection::Contains(Actor* a) const {
  // L86-89
  return std::find(vec_actors_.begin(), vec_actors_.end(), a) !=
         vec_actors_.end();
}

void Selection::Combine(World& world, std::span<Actor* const> new_selection,
                        bool is_combine, bool is_click) {
  // L91-145
  if (is_click) {
    // TODO: select BEST, not FIRST(上游注释)
    const std::span<Actor* const> adj_new_selection =
        new_selection.first(std::min<std::size_t>(new_selection.size(), 1));
    if (is_combine) {
      // SymmetricExceptWith:交集移除,补集加入
      for (Actor* a : adj_new_selection) {
        const auto it = std::find(vec_actors_.begin(), vec_actors_.end(), a);
        if (it != vec_actors_.end())
          vec_actors_.erase(it);
        else
          vec_actors_.push_back(a);
      }
    } else {
      vec_actors_.clear();
      vec_actors_.assign(adj_new_selection.begin(), adj_new_selection.end());
    }
  } else {
    if (is_combine) {
      // UnionWith
      for (Actor* a : new_selection)
        if (std::find(vec_actors_.begin(), vec_actors_.end(), a) ==
            vec_actors_.end())
          vec_actors_.push_back(a);
    } else {
      vec_actors_.clear();
      vec_actors_.assign(new_selection.begin(), new_selection.end());
    }
  }

  UpdateHash();

  for (Actor* a : new_selection)
    for (auto* sel : a->TraitsImplementing<INotifySelected>())
      sel->Selected(*a);

  RunUnsyncedWorld(&world, [&] {
    if (world.OrderGenerator() != nullptr)
      world.OrderGenerator()->SelectionChanged(world, vec_actors_);
  });
  for (auto* ns : vec_world_notify_selection_)
    ns->SelectionChanged();

  if (world.IsGameOver())
    return;

  // Play the selection voice from one of the selected actors(上游注释)
  for (Actor* actor : vec_actors_) {
    if (std::find(new_selection.begin(), new_selection.end(), actor) ==
        new_selection.end())
      continue;  // actors.Intersect(newSelectionCollection)
    if (actor->Owner() != world.LocalPlayer() || !actor->IsInWorld())
      continue;

    const game::ActorInfo* info = actor->Info();
    if (info == nullptr)
      continue;
    // ISelectableInfo 的 Voice 字段读取(TraitInfoOrDefault<ISelectableInfo>)
    const meta::RecordObject* selectable =
        FindTraitInfoOfInterface(*info, "OpenRA.Traits.ISelectableInfo");
    if (selectable == nullptr)
      continue;
    const std::string str_voice{
        RecordFieldString(*selectable, "Voice").value_or(std::string_view{})};
    if (fn_has_voice_ && !fn_has_voice_(*actor, str_voice))
      continue;
    if (fn_play_voice_)
      fn_play_voice_(*actor, str_voice);
    break;
  }
}

void Selection::Clear() {
  // L147-154
  vec_actors_.clear();
  UpdateHash();
  World& world = *ptr_world_;
  RunUnsyncedWorld(
      &world,
      [&] {
        if (world.OrderGenerator() != nullptr)
          world.OrderGenerator()->SelectionChanged(world, vec_actors_);
      });
  for (auto* ns : vec_world_notify_selection_)
    ns->SelectionChanged();
}

void Selection::SetRollover(std::span<Actor* const> rollover) {
  // L156-160
  vec_rollover_actors_.assign(rollover.begin(), rollover.end());
}

bool Selection::RolloverContains(Actor* a) const {
  // L162-165
  return std::find(vec_rollover_actors_.begin(), vec_rollover_actors_.end(),
                   a) != vec_rollover_actors_.end();
}

void Selection::Tick(Actor&) {
  // L167-177:出局/迷雾移除(FogObscures 的 RenderPlayer/Shroud 面随 Shroud
  // 批;RenderPlayer 缺省恒空 = 上游同样只剩 IsInWorld 条件)
  const std::size_t removed = std::erase_if(vec_actors_, [](Actor* a) {
    return !a->IsInWorld();
  });
  if (removed > 0) {
    UpdateHash();
    World& world = *ptr_world_;
    RunUnsyncedWorld(
        &world,
        [&] {
          if (world.OrderGenerator() != nullptr)
            world.OrderGenerator()->SelectionChanged(world, vec_actors_);
        });
    for (auto* ns : vec_world_notify_selection_)
      ns->SelectionChanged();
  }
}

std::vector<yaml::MiniYamlNode> Selection::IssueTraitData(Actor&) {
  // L179-185:FieldSaver.FormatValue(uint[]) —— 逗号列表
  std::string str_ids;
  for (std::size_t i = 0; i < vec_actors_.size(); i++) {
    if (i != 0)
      str_ids += ", ";
    str_ids += std::format("{}", vec_actors_[i]->ActorID());
  }
  yaml::StringPool& pool = yaml::MiniYaml::GlobalPool();
  yaml::MiniYamlNode node{pool.Intern("Selection"),
                          yaml::MiniYaml{pool.Intern(str_ids)}};
  return {node};
}

void Selection::ResolveTraitData(Actor& self, const yaml::MiniYaml& data) {
  // L187-196
  const yaml::MiniYamlNode* selection_node =
      data.NodeWithKeyOrDefault("Selection");
  if (selection_node != nullptr) {
    const std::string_view sv_value =
        selection_node->Value.Value != nullptr
            ? std::string_view{*selection_node->Value.Value}
            : std::string_view{};
    std::vector<Actor*> selected;
    for (const std::string_view part : meta::SplitComma(sv_value)) {
      // FieldLoader.GetValue<uint[]>(…);失败段 = 上游 uint.Parse 抛
      const std::uint32_t id = ParseUIntInvariant(part);
      if (Actor* a = self.world().GetActorById(id))
        selected.push_back(a);
    }
    Combine(self.world(), selected, false, false);
  }
}

}  // namespace ora::sim
