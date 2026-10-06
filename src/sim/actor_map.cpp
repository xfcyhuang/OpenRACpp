// UPSTREAM: OpenRA.Mods.Common/Traits/World/ActorMap.cs @b6fc03f(实现部分)
//          The implementation half of ActorMap.cs.
import std;

#include "sim/actor_map.hpp"

#include "game/actor_info.hpp"
#include "game/ruleset.hpp"
#include "map/map.hpp"
#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "sim/actor.hpp"
#include "sim/world.hpp"

namespace ora::sim {

// ———— CellTrigger(L45-90)————
class CellTrigger final {
 public:
  CellTrigger(std::vector<CPos> footprint, std::function<void(Actor&)> on_entry,
              std::function<void(Actor&)> on_exit)
      : footprint_{std::move(footprint)},
        fn_on_actor_entered_{std::move(on_entry)},
        fn_on_actor_exited_{std::move(on_exit)} {
    // Notify any actors that are initially inside the trigger zone(上游
    // 注释)| (upstream comment).
    dirty_ = true;
  }

  void Tick(ActorMap& actor_map) {
    if (!dirty_)
      return;

    // PERF: Reuse collection to avoid allocations.(上游注释)
    vec_old_actors_.clear();
    UnionWith(vec_old_actors_, vec_current_actors_);

    vec_current_actors_.clear();
    for (const CPos& cell : footprint_) {
      std::vector<Actor*> at = actor_map.GetActorsAt(cell);
      UnionWith(vec_current_actors_, at);
    }

    if (fn_on_actor_entered_ != nullptr)
      for (Actor* a : vec_current_actors_)
        if (!ContainsActor(vec_old_actors_, a))
          fn_on_actor_entered_(*a);

    if (fn_on_actor_exited_ != nullptr)
      for (Actor* a : vec_old_actors_)
        if (!ContainsActor(vec_current_actors_, a))
          fn_on_actor_exited_(*a);

    dirty_ = false;
  }

  bool dirty_ = false;  // L48 | (L48).
  const std::vector<CPos> footprint_;

 private:
  using ActorSet = std::vector<Actor*>;  // HashSet → 插入序 vector + id 判重

  static bool ContainsActor(const ActorSet& set, Actor* a) {
    const std::uint32_t id = a->ActorID();
    return std::any_of(set.begin(), set.end(),
                       [&](Actor* x) { return x->ActorID() == id; });
  }

  static void UnionWith(ActorSet& into, const std::vector<Actor*>& from) {
    for (Actor* a : from)
      if (!ContainsActor(into, a))
        into.push_back(a);
  }

  std::function<void(Actor&)> fn_on_actor_entered_;
  std::function<void(Actor&)> fn_on_actor_exited_;
  ActorSet vec_old_actors_;
  ActorSet vec_current_actors_;
};

// ———— ProximityTrigger(L92-165)————
class ProximityTrigger final {
 public:
  ProximityTrigger(const WPos& pos, const WDist& range, const WDist& v_range,
                   std::function<void(Actor&)> on_entry,
                   std::function<void(Actor&)> on_exit)
      : fn_on_actor_entered_{std::move(on_entry)},
        fn_on_actor_exited_{std::move(on_exit)} {
    Update(pos, range, v_range);
  }

  WPos TopLeft() const { return wpos_top_left_; }          // L94
  WPos BottomRight() const { return wpos_bottom_right_; }  // L95

  bool dirty_ = false;  // L97

  /// Update(L116-128) | Update (L116-128).
  void Update(const WPos& new_pos, const WDist& new_range,
              const WDist& new_v_range) {
    wpos_position_ = new_pos;
    wdist_range_ = new_range;
    wdist_v_range_ = new_v_range;

    const WVec offset{new_range, new_range, new_v_range};

    wpos_top_left_ = new_pos - offset;
    wpos_bottom_right_ = new_pos + offset;

    dirty_ = true;
  }

  /// Tick(L130-157) | Tick (L130-157).
  void Tick(ActorMap& am) {
    if (!dirty_)
      return;

    // PERF: Reuse collection to avoid allocations.(上游注释)
    vec_old_actors_.clear();
    UnionWith(vec_old_actors_, vec_current_actors_);

    const WVec delta{wdist_range_, wdist_range_, WDist{0}};
    vec_current_actors_.clear();
    for (Actor* a : am.ActorsInBox(wpos_position_ - delta,
                                   wpos_position_ + delta)) {
      // Where 谓词(L143-144)| the Where predicate (L143-144).
      if ((a->CenterPosition() - wpos_position_).HorizontalLengthSquared() <
              wdist_range_.LengthSquared() &&
          (wdist_v_range_.Length == 0 ||
           am.world_.Map().DistanceAboveTerrain(a->CenterPosition())
                   .LengthSquared() <= wdist_v_range_.LengthSquared()))
        if (!ContainsActor(vec_current_actors_, a))
          vec_current_actors_.push_back(a);
    }

    if (fn_on_actor_entered_ != nullptr)
      for (Actor* a : vec_current_actors_)
        if (!ContainsActor(vec_old_actors_, a))
          fn_on_actor_entered_(*a);

    if (fn_on_actor_exited_ != nullptr)
      for (Actor* a : vec_old_actors_)
        if (!ContainsActor(vec_current_actors_, a))
          fn_on_actor_exited_(*a);

    dirty_ = false;
  }

  /// Dispose(L159-164) | Dispose (L159-164).
  void Dispose() {
    if (fn_on_actor_exited_ != nullptr)
      for (Actor* a : vec_current_actors_)
        fn_on_actor_exited_(*a);
  }

 private:
  using ActorSet = std::vector<Actor*>;

  static bool ContainsActor(const ActorSet& set, Actor* a) {
    const std::uint32_t id = a->ActorID();
    return std::any_of(set.begin(), set.end(),
                       [&](Actor* x) { return x->ActorID() == id; });
  }

  static void UnionWith(ActorSet& into, const std::vector<Actor*>& from) {
    for (Actor* a : from)
      if (!ContainsActor(into, a))
        into.push_back(a);
  }

  friend class ActorMap;  // dirty_ 直写面(Add/RemovePosition 触发)
                          // the dirty_ direct-write face.

  std::function<void(Actor&)> fn_on_actor_entered_;
  std::function<void(Actor&)> fn_on_actor_exited_;
  ActorSet vec_old_actors_;
  ActorSet vec_current_actors_;

  WPos wpos_position_{};
  WDist wdist_range_{0};
  WDist wdist_v_range_{0};
  WPos wpos_top_left_{};
  WPos wpos_bottom_right_{};
};

namespace {

/// IHitShape.OuterRadius 的值袋求值(形状嵌套记录;Circle=Radius,
/// Rectangle=corners 最大长度,Capsule=Radius+max(|A|,|B|))
/// The bag evaluation of IHitShape.OuterRadius (the nested shape record;
/// Circle = Radius, Rectangle = the max corner length, Capsule = Radius +
/// max(|A|,|B|)).
WDist OuterRadiusOfShape(const meta::RecordObject& rec_shape) {
  const auto* generated =
      dynamic_cast<const meta::GeneratedRecord*>(&rec_shape);
  if (generated == nullptr)
    return WDist{0};
  const std::string_view name = generated->record_desc().str_name;

  const auto read_ints = [&](std::string_view field) -> std::array<std::int64_t, 2> {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++) {
      if (fields[i]->str_name != field)
        continue;
      const meta::GenericValue& v = generated->Slot(i);
      if (auto* t = std::get_if<meta::GenericTuple>(&v.val))
        return {t->arr_ints[0], t->arr_ints[1]};  // WDist.Length / int2
    }
    return {0, 0};
  };

  const auto int2_length = [&](std::array<std::int64_t, 2> p) {
    return ISqrt(p[0] * p[0] + p[1] * p[1]);
  };

  if (name == "CircleShape")
    return WDist{static_cast<std::int32_t>(read_ints("Radius")[0])};
  if (name == "RectangleShape") {
    // corners.Max(x => x.Length):四角 = 两点横纵分量的组合极值
    // corners.Max(x => x.Length): the corners' extreme = the component-wise
    // extremes of the two points.
    const std::int64_t a = int2_length(read_ints("TopLeft"));
    const std::int64_t b = int2_length(read_ints("BottomRight"));
    return WDist{static_cast<std::int32_t>(std::max(a, b))};
  }
  if (name == "CapsuleShape") {
    // OuterRadius = Radius + max(PointA.Length, PointB.Length)(Capsule.cs
    // L62)| (Capsule.cs L62).
    const std::int64_t a = int2_length(read_ints("PointA"));
    const std::int64_t b = int2_length(read_ints("PointB"));
    return WDist{static_cast<std::int32_t>(
        read_ints("Radius")[0] + std::max(a, b))};
  }
  // 未知形状:Initialize 未接的运行时面(见 loaders.cpp 注记)—— 0 贡献
  // An unknown shape: the runtime face noted un-wired in loaders.cpp — a
  // 0 contribution.
  return WDist{0};
}

/// 生成记录的按名嵌套记录槽(Type 字段 → 形状记录)
/// The by-name nested-record slot of a generated record (the Type field →
/// the shape record).
const meta::RecordObject* ShapeSlotOf(const meta::RecordObject& rec) {
  const auto* generated = dynamic_cast<const meta::GeneratedRecord*>(&rec);
  if (generated == nullptr)
    return nullptr;
  const std::vector<const meta::FieldDesc*> fields =
      meta::CollectFields(generated->record_desc());
  for (std::size_t i = 0; i < fields.size(); i++) {
    if (fields[i]->str_name != "Type")
      continue;
    const meta::GenericValue& v = generated->Slot(i);
    if (auto* r =
            std::get_if<std::shared_ptr<meta::RecordObject>>(&v.val))
      return r->get();
  }
  return nullptr;
}

}  // namespace

// ———— ctor(L191-210)————
ActorMap::ActorMap(World& world, int bin_size)
    : bin_size_{bin_size}, map_{world.Map()}, world_{world} {
  // influence = [new CellLayer<InfluenceNode>(world.Map)](L195:0 层恒有)
  // (L195: layer 0 always present.)
  vec_influence_.push_back(std::make_unique<InfluenceLayer>(map_));

  const Size size = map_.MapSize();
  cols_ = CellCoordToBinIndex(size.Width) + 1;   // L197
  rows_ = CellCoordToBinIndex(size.Height) + 1;  // L198
  vec_bins_.resize(static_cast<std::size_t>(rows_) * cols_);  // L199-202

  // LargestActorRadius/ LargestBlockingActorRadius(L207-209)
  std::optional<WDist> largest_actor_radius;
  std::vector<const game::ActorInfo*> blockers;
  for (const auto& [name, info] : map_.Rules().Actors()) {
    for (const meta::RecordObject* rec :
         info->TraitInfosByInterface(
             "OpenRA.Traits.ITraitInfoInterface")) {
      if (rec->record_desc().str_name != "HitShapeInfo")
        continue;
      if (const meta::RecordObject* shape = ShapeSlotOf(*rec)) {
        const WDist r = OuterRadiusOfShape(*shape);
        if (!largest_actor_radius || r.Length > largest_actor_radius->Length)
          largest_actor_radius = r;
      }
    }
    if (info->HasTraitInfoOfInterface(
            "OpenRA.Traits.IBlocksProjectilesInfo"))
      blockers.push_back(info.get());
  }
  // SelectMany(...).Max():空序列 InvalidOperationException 等价(规则表
  // 至少含 world actor,实际可达面恒非空)
  // SelectMany(...).Max(): the empty-sequence InvalidOperationException
  // equivalent (the rules table always carries the world actor — the
  // reachable face is never empty).
  wdist_largest_actor_radius_ = largest_actor_radius.value_or(WDist{0});
  if (!blockers.empty()) {
    std::optional<WDist> largest_blocking;
    for (const game::ActorInfo* info : blockers)
      for (const meta::RecordObject* rec :
           info->TraitInfosByInterface(
               "OpenRA.Traits.ITraitInfoInterface")) {
        if (rec->record_desc().str_name != "HitShapeInfo")
          continue;
        if (const meta::RecordObject* shape = ShapeSlotOf(*rec)) {
          const WDist r = OuterRadiusOfShape(*shape);
          if (!largest_blocking || r.Length > largest_blocking->Length)
            largest_blocking = r;
        }
      }
    wdist_largest_blocking_actor_radius_ = largest_blocking.value_or(WDist{0});
  }
}

ActorMap::~ActorMap() = default;

// ———— INotifyCreated.Created(L212-227)————
void ActorMap::Created(Actor& self) {
  const auto custom_movement_layers =
      self.TraitsImplementing<ICustomMovementLayer>();
  if (custom_movement_layers.empty())
    return;

  std::uint8_t max_index = 0;
  for (const auto* cml : custom_movement_layers)
    max_index = std::max(max_index, cml->Index());
  const std::size_t length = static_cast<std::size_t>(max_index) + 1;

  vec_custom_movement_layers_.resize(length, nullptr);
  vec_influence_.resize(length);

  for (auto* cml : custom_movement_layers) {
    const std::size_t index = cml->Index();
    vec_custom_movement_layers_[index] = cml;
    if (vec_influence_[index] == nullptr)
      vec_influence_[index] =
          std::make_unique<InfluenceLayer>(self.world().Map());
  }
}

// ———— 查询族(L265-287)————
std::vector<Actor*> ActorMap::GetActorsAt(CPos a) {
  // PERF: Custom enumerator for efficiency - using `yield` is slower.
  // (上游注释;C++ 侧 = ForEachActorAt 物化 + 零分配核)
  // PERF: Custom enumerator for efficiency - using `yield` is slower.
  // (upstream comment; the C++ side = a materialized ForEachActorAt plus
  // the zero-allocation core)
  std::vector<Actor*> out;
  ForEachActorAt(a, [&](Actor& actor) { out.push_back(&actor); });
  return out;
}

std::vector<Actor*> ActorMap::GetActorsAt(CPos a, SubCell sub) {
  const MPos uv = a.ToMPos(map_.Grid().Type);
  auto* layer = LayerAt(a.Layer());
  std::vector<Actor*> out;
  if (layer == nullptr || !layer->Contains(uv))
    return out;

  const bool always = sub == SubCell::FullCell || sub == SubCell::Any;
  for (InfluenceNode* i = layer->Get(uv); i != nullptr; i = i->next)
    if (i->sub_cell == sub || i->sub_cell == SubCell::FullCell || always)
      out.push_back(i->actor);
  return out;
}

// ———— HasFreeSubCell/FreeSubCell(L289-334)————
bool ActorMap::HasFreeSubCell(CPos cell, bool check_transient) {
  return FreeSubCell(cell, SubCell::Any, check_transient) != SubCell::Invalid;
}

SubCell ActorMap::FreeSubCell(CPos cell, SubCell preferred_sub_cell,
                              bool check_transient) {
  const MPos uv = cell.ToMPos(map_.Grid().Type);
  auto* layer = LayerAt(cell.Layer());
  if (layer == nullptr || !layer->Contains(uv))
    return preferred_sub_cell != SubCell::Any ? preferred_sub_cell
                                              : SubCell::First;

  InfluenceNode* influence_node = layer->Get(uv);
  if (preferred_sub_cell != SubCell::Any &&
      !static_cast<bool>(
          AnyActorsAtInner(influence_node, cell, preferred_sub_cell,
                           check_transient)))
    return preferred_sub_cell;

  if (influence_node == nullptr)
    return static_cast<SubCell>(map_.Grid().DefaultSubCell());

  for (int i = static_cast<int>(SubCell::First);
       i < static_cast<int>(map_.Grid().kSubCellOffsets.size()); i++)
    if (i != static_cast<int>(preferred_sub_cell) &&
        !AnyActorsAtInner(influence_node, cell, static_cast<SubCell>(i),
                          check_transient))
      return static_cast<SubCell>(i);

  return SubCell::Invalid;
}

SubCell ActorMap::FreeSubCell(
    CPos cell, SubCell preferred_sub_cell,
    const std::function<bool(Actor&)>& check_if_blocker) {
  const MPos uv = cell.ToMPos(map_.Grid().Type);
  auto* layer = LayerAt(cell.Layer());
  if (layer == nullptr || !layer->Contains(uv))
    return preferred_sub_cell != SubCell::Any ? preferred_sub_cell
                                              : SubCell::First;

  InfluenceNode* influence_node = layer->Get(uv);
  if (preferred_sub_cell != SubCell::Any &&
      !AnyActorsAtInner(influence_node, preferred_sub_cell, check_if_blocker))
    return preferred_sub_cell;

  if (influence_node == nullptr)
    return static_cast<SubCell>(map_.Grid().DefaultSubCell());

  for (int i = static_cast<int>(SubCell::First);
       i < static_cast<int>(map_.Grid().kSubCellOffsets.size()); i++)
    if (i != static_cast<int>(preferred_sub_cell) &&
        AnyActorsAtInner(influence_node, static_cast<SubCell>(i),
                         check_if_blocker) == false)
      return static_cast<SubCell>(i);

  return SubCell::Invalid;
}

// ———— AnyActorsAt 族(L336-399)————
bool ActorMap::AnyActorsAt(CPos a) {
  // NOTE: always includes transients with influence(上游注释)
  const MPos uv = a.ToMPos(map_.Grid().Type);
  auto* layer = LayerAt(a.Layer());
  if (layer == nullptr || !layer->Contains(uv))
    return false;
  return layer->Get(uv) != nullptr;
}

bool ActorMap::AnyActorsAt(CPos a, SubCell sub, bool check_transient) {
  const MPos uv = a.ToMPos(map_.Grid().Type);
  auto* layer = LayerAt(a.Layer());
  if (layer == nullptr || !layer->Contains(uv))
    return false;
  return AnyActorsAtInner(layer->Get(uv), a, sub, check_transient);
}

bool ActorMap::AnyActorsAt(CPos a, SubCell sub,
                           const std::function<bool(Actor&)>& with_condition) {
  const MPos uv = a.ToMPos(map_.Grid().Type);
  auto* layer = LayerAt(a.Layer());
  if (layer == nullptr || !layer->Contains(uv))
    return false;
  return AnyActorsAtInner(layer->Get(uv), sub, with_condition);
}

// ———— AllActors(L401-411)————
std::vector<Actor*> ActorMap::AllActors() {
  std::vector<Actor*> out;
  for (const auto& layer : vec_influence_) {
    if (layer == nullptr)
      continue;
    // 逐格链表游走(CellLayer 全枚举 = Entries 行主序)
    // The per-cell list walk (the CellLayer full enumeration = the
    // Entries' row-major order).
    for (InfluenceNode* node : layer->Entries())
      for (InfluenceNode* i = node; i != nullptr; i = i->next)
        out.push_back(i->actor);
  }
  return out;
}

// ———— AddInfluence/RemoveInfluence(L413-451)————
void ActorMap::AddInfluence(Actor* self, IOccupySpace* ios) {
  for (const auto& [cell, sub_cell] : ios->OccupiedCells()) {
    const MPos uv = cell.ToMPos(map_.Grid().Type);
    auto* layer = LayerAt(cell.Layer());
    if (layer == nullptr || !layer->Contains(uv))
      continue;

    InfluenceNode* node = new InfluenceNode{layer->Get(uv), sub_cell, self};
    layer->Set(uv, node);

    if (auto it = map_cell_trigger_influence_.find(cell);
        it != map_cell_trigger_influence_.end())
      for (auto* t : it->second)
        t->dirty_ = true;

    for (const auto& fn : vec_cell_updated_)
      fn(cell);
  }
}

void ActorMap::RemoveInfluence(Actor* self, IOccupySpace* ios) {
  for (const auto& [cell, sub_cell] : ios->OccupiedCells()) {
    const MPos uv = cell.ToMPos(map_.Grid().Type);
    auto* layer = LayerAt(cell.Layer());
    if (layer == nullptr || !layer->Contains(uv))
      continue;

    InfluenceNode* temp = RemoveInfluenceInner(layer->Get(uv), self);
    layer->Set(uv, temp);

    if (auto it = map_cell_trigger_influence_.find(cell);
        it != map_cell_trigger_influence_.end())
      for (auto* t : it->second)
        t->dirty_ = true;

    for (const auto& fn : vec_cell_updated_)
      fn(cell);
  }
}

ActorMap::InfluenceNode* ActorMap::RemoveInfluenceInner(InfluenceNode* head,
                                                        Actor* to_remove) {
  // 递归 ref 重写 → 迭代重建(摘除节点 delete = GC 的显式等价,链序保持)
  // The recursive ref rewrite → an iterative rebuild (deleting the removed
  // nodes = the explicit GC equivalent; list order preserved).
  InfluenceNode* out_head = nullptr;
  InfluenceNode** out_tail = &out_head;
  InfluenceNode* node = head;
  while (node != nullptr) {
    InfluenceNode* next = node->next;
    if (node->actor == to_remove)
      delete node;
    else {
      *out_tail = node;
      node->next = nullptr;
      out_tail = &node->next;
    }
    node = next;
  }
  return out_head;
}

// ———— AnyActorsAt 的静态内层(L348-365/L379-388)————
bool ActorMap::AnyActorsAtInner(InfluenceNode* influence_node, CPos a,
                                SubCell sub, bool check_transient) {
  // NOTE: pos required to be in map bounds(上游注释)
  const bool always = sub == SubCell::FullCell || sub == SubCell::Any;
  for (InfluenceNode* i = influence_node; i != nullptr; i = i->next) {
    if (always || i->sub_cell == sub || i->sub_cell == SubCell::FullCell) {
      if (check_transient)
        return true;

      // PERF: Avoid trait lookup(L359):上游
      // `i.Actor.OccupiesSpace is not IPositionable pos ||
      //  !pos.IsLeavingCell(a, i.SubCell)` —— IPositionable 实现随 Mobile
      // 批落地;部分覆盖装配面下 `is not` 恒真 → 阻挡(COVERAGE 登记)
      // PERF: Avoid trait lookup (L359): with the IPositionable family
      // landing in the Mobile batch, the partial-coverage assembly face
      // makes `is not` always true → blocked (in COVERAGE).
      return true;
    }
  }
  return false;
}

bool ActorMap::AnyActorsAtInner(
    InfluenceNode* influence_node, SubCell sub,
    const std::function<bool(Actor&)>& with_condition) {
  const bool always = sub == SubCell::FullCell || sub == SubCell::Any;

  for (InfluenceNode* i = influence_node; i != nullptr; i = i->next)
    if ((always || i->sub_cell == sub || i->sub_cell == SubCell::FullCell) &&
        with_condition(*i->actor))
      return true;

  return false;
}

// ———— UpdateOccupiedCells(L464-471)————
void ActorMap::UpdateOccupiedCells(IOccupySpace* ios) {
  if (vec_cell_updated_.empty())
    return;
  for (const auto& [cell, sub_cell] : ios->OccupiedCells())
    for (const auto& fn : vec_cell_updated_)
      fn(cell);
}

// ———— ITick.Tick(L473-509)————
void ActorMap::Tick(Actor& self) {
  // Position updates are done in one pass(上游注释)
  if (!vec_remove_actor_position_.empty()) {
    for (Bin& bin : vec_bins_) {
      // RemoveAll(actorShouldBeRemoved)(L481)| RemoveAll (L481).
      const auto before = bin.actors.size();
      const auto& to_remove = vec_remove_actor_position_;
      bin.actors.erase(
          std::remove_if(bin.actors.begin(), bin.actors.end(),
                         [&](Actor* a) {
                           const std::uint32_t id = a->ActorID();
                           return std::any_of(
                               to_remove.begin(), to_remove.end(),
                               [&](Actor* r) { return r->ActorID() == id; });
                         }),
          bin.actors.end());
      if (bin.actors.size() != before)
        for (auto* t : bin.proximity_triggers)
          t->dirty_ = true;
    }
    vec_remove_actor_position_.clear();
  }

  for (Actor* a : vec_add_actor_position_) {
    const WPos pos = a->CenterPosition();
    const int col = std::clamp(WorldCoordToBinIndex(pos.X), 0, cols_ - 1);
    const int row = std::clamp(WorldCoordToBinIndex(pos.Y), 0, rows_ - 1);
    Bin& bin = BinAt(row, col);

    bin.actors.push_back(a);
    for (auto* t : bin.proximity_triggers)
      t->dirty_ = true;
  }
  vec_add_actor_position_.clear();

  // Dictionary 枚举序 = 插入序(无删除面)—— cellTriggers/proximityTriggers
  // 以 unordered_map 承载;tick 顺序经 nextTriggerId 单调键的有序视图保持
  // (插入序 = id 升序,与上游 Dictionary 桶序一致)
  // The Dictionary enumeration order = insertion order (no deletion face)
  // — carried by unordered_maps keyed by the monotonic nextTriggerId; an
  // ordered view keeps the tick order (insertion order = ascending id,
  // matching upstream's Dictionary bucket order).
  std::vector<int> ids;
  ids.reserve(map_cell_triggers_.size());
  for (const auto& [id, t] : map_cell_triggers_)
    ids.push_back(id);
  std::sort(ids.begin(), ids.end());
  for (int id : ids)
    map_cell_triggers_[static_cast<std::size_t>(id)]->Tick(*this);

  ids.clear();
  for (const auto& [id, t] : map_proximity_triggers_)
    ids.push_back(id);
  std::sort(ids.begin(), ids.end());
  for (int id : ids)
    map_proximity_triggers_[static_cast<std::size_t>(id)]->Tick(*this);
}

// ———— 触发器族(L511-586)————
int ActorMap::AddCellTrigger(const std::vector<CPos>& cells,
                             std::function<void(Actor&)> on_entry,
                             std::function<void(Actor&)> on_exit) {
  const int id = next_trigger_id_++;
  auto t = std::make_unique<CellTrigger>(cells, std::move(on_entry),
                                         std::move(on_exit));
  CellTrigger* raw = t.get();
  map_cell_triggers_.emplace(id, std::move(t));

  auto* layer = LayerAt(0);
  for (const CPos& c : cells) {
    if (layer == nullptr || !layer->Contains(c))
      continue;
    map_cell_trigger_influence_[c].push_back(raw);
  }

  return id;
}

std::vector<CPos> ActorMap::TriggerPositions() {
  std::vector<CPos> out;
  out.reserve(map_cell_trigger_influence_.size());
  for (const auto& [cell, triggers] : map_cell_trigger_influence_)
    out.push_back(cell);
  return out;
}

void ActorMap::RemoveCellTrigger(int id) {
  auto it = map_cell_triggers_.find(id);
  if (it == map_cell_triggers_.end())
    return;
  CellTrigger* trigger = it->second.get();

  for (const CPos& c : trigger->footprint_) {
    auto infl = map_cell_trigger_influence_.find(c);
    if (infl == map_cell_trigger_influence_.end())
      continue;
    // RemoveAll(t => t == trigger) | RemoveAll (t => t == trigger).
    std::erase(infl->second, trigger);
  }
}

int ActorMap::AddProximityTrigger(const WPos& pos, const WDist& range,
                                  const WDist& v_range,
                                  std::function<void(Actor&)> on_entry,
                                  std::function<void(Actor&)> on_exit) {
  const int id = next_trigger_id_++;
  auto t = std::make_unique<ProximityTrigger>(pos, range, v_range,
                                              std::move(on_entry),
                                              std::move(on_exit));
  ProximityTrigger* raw = t.get();
  map_proximity_triggers_.emplace(id, std::move(t));

  for (Bin* bin_ptr : BinsInBox(raw->TopLeft(), raw->BottomRight()))
    bin_ptr->proximity_triggers.push_back(raw);

  return id;
}

void ActorMap::RemoveProximityTrigger(int id) {
  auto it = map_proximity_triggers_.find(id);
  if (it == map_proximity_triggers_.end())
    return;
  ProximityTrigger* t = it->second.get();

  for (Bin* bin_ptr : BinsInBox(t->TopLeft(), t->BottomRight()))
    std::erase(bin_ptr->proximity_triggers, t);

  t->Dispose();
  map_proximity_triggers_.erase(it);
}

void ActorMap::UpdateProximityTrigger(int id, const WPos& new_pos,
                                      const WDist& new_range,
                                      const WDist& new_v_range) {
  auto it = map_proximity_triggers_.find(id);
  if (it == map_proximity_triggers_.end())
    return;
  ProximityTrigger* t = it->second.get();

  for (Bin* bin_ptr : BinsInBox(t->TopLeft(), t->BottomRight()))
    std::erase(bin_ptr->proximity_triggers, t);

  t->Update(new_pos, new_range, new_v_range);

  for (Bin* bin_ptr : BinsInBox(t->TopLeft(), t->BottomRight()))
    bin_ptr->proximity_triggers.push_back(t);
}

// ———— 位置缓存面(L588-602)————
void ActorMap::AddPosition(Actor* a, IOccupySpace* ios) {
  // HashSet.Add:去重(ActorID)| HashSet.Add: dedup by ActorID.
  const std::uint32_t id = a->ActorID();
  if (!std::any_of(vec_add_actor_position_.begin(),
                   vec_add_actor_position_.end(),
                   [&](Actor* x) { return x->ActorID() == id; }))
    vec_add_actor_position_.push_back(a);
}

void ActorMap::RemovePosition(Actor* a, IOccupySpace* ios) {
  const std::uint32_t id = a->ActorID();
  if (!std::any_of(vec_remove_actor_position_.begin(),
                   vec_remove_actor_position_.end(),
                   [&](Actor* x) { return x->ActorID() == id; }))
    vec_remove_actor_position_.push_back(a);
}

void ActorMap::UpdatePosition(Actor* a, IOccupySpace* ios) {
  RemovePosition(a, ios);
  AddPosition(a, ios);
}

// ———— 几何辅助(L614-642)————
Rectangle ActorMap::BinRectangleCoveringWorldArea(int world_left,
                                                  int world_top,
                                                  int world_right,
                                                  int world_bottom) const {
  const int min_col = std::clamp(WorldCoordToBinIndex(world_left), 0, cols_ - 1);
  const int min_row = std::clamp(WorldCoordToBinIndex(world_top), 0, rows_ - 1);
  const int max_col = std::clamp(WorldCoordToBinIndex(world_right), 0, cols_ - 1);
  const int max_row = std::clamp(WorldCoordToBinIndex(world_bottom), 0, rows_ - 1);
  return Rectangle::FromLTRB(min_col, min_row, max_col, max_row);
}

std::vector<ActorMap::Bin*> ActorMap::BinsInBox(const WPos& a, const WPos& b) {
  const int left = std::min(a.X, b.X);
  const int top = std::min(a.Y, b.Y);
  const int right = std::max(a.X, b.X);
  const int bottom = std::max(a.Y, b.Y);
  const Rectangle region =
      BinRectangleCoveringWorldArea(left, top, right, bottom);
  std::vector<Bin*> out;
  for (int row = region.Top(); row <= region.Bottom(); row++)
    for (int col = region.Left(); col <= region.Right(); col++)
      out.push_back(&BinAt(row, col));
  return out;
}

// ———— ActorsInBox(L644-671)————
std::vector<Actor*> ActorMap::ActorsInBox(const WPos& a, const WPos& b) {
  // PERF: Inline BinsInBox here to avoid allocations as this method is
  // called often.(上游注释)
  std::vector<Actor*> out;
  const int left = std::min(a.X, b.X);
  const int top = std::min(a.Y, b.Y);
  const int right = std::max(a.X, b.X);
  const int bottom = std::max(a.Y, b.Y);
  const Rectangle region =
      BinRectangleCoveringWorldArea(left, top, right, bottom);
  for (int row = region.Top(); row <= region.Bottom(); row++) {
    for (int col = region.Left(); col <= region.Right(); col++) {
      for (Actor* actor : BinAt(row, col).actors) {
        if (actor->IsInWorld()) {
          const WPos c = actor->CenterPosition();
          if (left <= c.X && c.X <= right && top <= c.Y && c.Y <= bottom)
            out.push_back(actor);
        }
      }
    }
  }
  return out;
}

}  // namespace ora::sim
