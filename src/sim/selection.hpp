// UPSTREAM: OpenRA.Mods.Common/Traits/World/Selection.cs @b6fc03f L18-198
//          (逐语义重写)
//          Verbatim-semantics rewrite.
//
// 机制对照 / Mechanism mapping:
//  - HashSet<Actor>(插入序枚举 = Actors)→ 插入序 vector + ActorID 判重
//    (RemoveWhere 的枚举序 = HashSet 实现序在无删除时为插入序 —— 与上游
//    同为"插入序"消费面;同步哈希不含 Selection.Hash 本体,刷新序安全)
//    The HashSet<Actor> (its enumeration feeds Actors) becomes an
//    insertion-ordered vector with ActorID dedup.
//  - world.OrderGenerator.SelectionChanged 经 IOrderGenerator 面分发
//    world.OrderGenerator.SelectionChanged dispatches through the
//    IOrderGenerator face.
//  - 语音面(actor.HasVoice/PlayVoice,L138-144)随 VoiceInfo trait 批;
//    本批保留注入钩子(缺省 no-op —— 首个可选 actor 直接 break)
//    The voice face (L138-144) lands with the VoiceInfo trait batch; an
//    injection hook stands in this batch (the default no-ops — the first
//    selectable actor just breaks).
#pragma once
import std;

#include "sim/trait_interfaces.hpp"

namespace ora::sim {

class World;

/// ISelection(TraitsInterfaces.cs L494-506)
/// ISelection (TraitsInterfaces.cs L494-506).
class ISelection {
 public:
  static constexpr gen::TypeId kTypeId = gen::TypeId::OpenRA_Traits_ISelection;
  virtual ~ISelection() = default;
  virtual int Hash() const = 0;
  virtual std::span<Actor* const> Actors() const = 0;
  virtual void Add(Actor* a) = 0;
  virtual void Remove(Actor* a) = 0;
  virtual bool Contains(Actor* a) const = 0;
  virtual void Combine(World& world, std::span<Actor* const> new_selection,
                       bool is_combine, bool is_click) = 0;
  virtual void Clear() = 0;
  virtual bool RolloverContains(Actor* a) const = 0;
  virtual void SetRollover(std::span<Actor* const> rollover) = 0;
};

/// SelectionInfo(Selection.cs L18-21):无字段工厂面
/// SelectionInfo (Selection.cs L18-21): the field-free factory face.
class Selection final : public TraitBase,
                        public ISelection,
                        public INotifyCreated,
                        public INotifyOwnerChanged,
                        public ITick,
                        public IGameSaveTraitData {
 public:
  ORA_TRAIT_INTERFACES(Selection, OpenRA_Mods_Common_Traits_Selection,
                       ISelection, INotifyCreated, INotifyOwnerChanged, ITick,
                       IGameSaveTraitData)

  /// Hash(L26) | Hash (L26).
  int Hash() const override { return int4_hash_; }
  std::span<Actor* const> Actors() const override { return vec_actors_; }  // L27

  void Add(Actor* a) override;                       // L49-60
  void Remove(Actor* a) override;                    // L62-71
  bool Contains(Actor* a) const override;            // L86-89
  void Combine(World& world, std::span<Actor* const> new_selection,
               bool is_combine, bool is_click) override;  // L91-145
  void Clear() override;                             // L147-154
  bool RolloverContains(Actor* a) const override;    // L162-165
  void SetRollover(std::span<Actor* const> rollover) override;  // L156-160

  void Created(Actor& self) override;                // L35-39
  void OnOwnerChanged(Actor& a, Player& old_owner, Player& new_owner) override;  // L73-84
  void Tick(Actor& self) override;                   // L167-177

  std::vector<yaml::MiniYamlNode> IssueTraitData(Actor& self) override;          // L179-185
  void ResolveTraitData(Actor& self, const yaml::MiniYaml& data) override;       // L187-196

  /// 语音注入面(L138-144 的 HasVoice/PlayVoice;随 VoiceInfo 批换实体)
  /// The voice injection faces (L138-144's HasVoice/PlayVoice; replaced by
  /// the real face with the VoiceInfo batch).
  void SetVoiceSources(std::function<bool(Actor&, const std::string&)> fn_has,
                       std::function<void(Actor&, const std::string&)> fn_play) {
    fn_has_voice_ = std::move(fn_has);
    fn_play_voice_ = std::move(fn_play);
  }

 private:
  void UpdateHash() { ++int4_hash_; }  // L41-47(非真哈希 —— 刷新哨兵)

  int int4_hash_ = 0;
  std::vector<Actor*> vec_actors_;       // HashSet 语义的插入序承载
  std::vector<Actor*> vec_rollover_actors_;  // L30
  World* ptr_world_ = nullptr;           // L31
  std::vector<INotifySelection*> vec_world_notify_selection_;  // L33

  std::function<bool(Actor&, const std::string&)> fn_has_voice_;
  std::function<void(Actor&, const std::string&)> fn_play_voice_;
};

}  // namespace ora::sim
