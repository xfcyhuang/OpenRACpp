#pragma once
import std;

#include "core/cell_pos.hpp"
#include "sim/actor.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::sim {
class ActorInitializer;
}

namespace ora::mods {

struct MapSmudge {
  std::string str_type;
  int int4_depth = 0;
};

struct SmudgeLayerInfoData {
  std::string str_type{"Scorch"};
  std::string str_sequence{"scorch"};
  int int4_smoke_chance = 0;
  WDist max_smoke_offset_distance{0};
  std::string str_smoke_image;
  std::vector<std::string> vec_smoke_sequences;
  std::string str_smoke_palette{"effect"};
  std::string str_palette{"terrain"};
  std::vector<std::pair<CPos, MapSmudge>> vec_initial_smudges;

  static SmudgeLayerInfoData Parse(const meta::RecordObject& rec_info);
};

class SmudgeLayer final : public sim::TraitBase,
                          public sim::INotifyActorDisposing {
 public:
  ORA_TRAIT_INTERFACES(
      SmudgeLayer, OpenRA_Mods_Common_Traits_SmudgeLayer,
      sim::INotifyActorDisposing)

  SmudgeLayer(sim::Actor& self, SmudgeLayerInfoData info);

  void AddSmudge(CPos loc);

  void RemoveSmudge(CPos loc);

  const SmudgeLayerInfoData& Info() const { return info_; }

 private:

  void Disposing(sim::Actor& self) override;

  struct Smudge {
    std::string str_type;
    int int4_depth = 0;
    bool b_deleted = false;

  };

  SmudgeLayerInfoData info_;
  sim::World& world_;
  std::map<CPos, Smudge> map_tiles_;
  std::map<CPos, Smudge> map_dirty_;
  bool b_disposed_ = false;
};

}
