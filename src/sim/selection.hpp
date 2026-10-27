#pragma once
import std;

#include "sim/trait_interfaces.hpp"

namespace ora::sim {

class World;

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

  int Hash() const override { return int4_hash_; }
  std::span<Actor* const> Actors() const override { return vec_actors_; }

  void Add(Actor* a) override;
  void Remove(Actor* a) override;
  bool Contains(Actor* a) const override;
  void Combine(World& world, std::span<Actor* const> new_selection,
               bool is_combine, bool is_click) override;
  void Clear() override;
  bool RolloverContains(Actor* a) const override;
  void SetRollover(std::span<Actor* const> rollover) override;

  void Created(Actor& self) override;
  void OnOwnerChanged(Actor& a, Player& old_owner, Player& new_owner) override;
  void Tick(Actor& self) override;

  std::vector<yaml::MiniYamlNode> IssueTraitData(Actor& self) override;
  void ResolveTraitData(Actor& self, const yaml::MiniYaml& data) override;

  void SetVoiceSources(std::function<bool(Actor&, const std::string&)> fn_has,
                       std::function<void(Actor&, const std::string&)> fn_play) {
    fn_has_voice_ = std::move(fn_has);
    fn_play_voice_ = std::move(fn_play);
  }

 private:
  void UpdateHash() { ++int4_hash_; }

  int int4_hash_ = 0;
  std::vector<Actor*> vec_actors_;
  std::vector<Actor*> vec_rollover_actors_;
  World* ptr_world_ = nullptr;
  std::vector<INotifySelection*> vec_world_notify_selection_;

  std::function<bool(Actor&, const std::string&)> fn_has_voice_;
  std::function<void(Actor&, const std::string&)> fn_play_voice_;
};

}
