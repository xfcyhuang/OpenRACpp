#pragma once
import std;

#include "core/wangle.hpp"
#include "meta/generic_record.hpp"
#include "core/wrot.hpp"
#include "core/wvec.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/sync_hash.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::game {
class ActorInfo;
}

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::ISync;
using sim::TraitBase;

struct BodyOrientationInfoData {
  int int4_quantized_facings = -1;
  WAngle angle_camera_pitch = WAngle::FromDegrees(40);
  bool b_use_classic_perspective_fudge = true;

  static BodyOrientationInfoData Parse(const meta::RecordObject& rec_info);

  WVec LocalToWorld(const WVec& vec) const;

  WRot QuantizeOrientation(const WRot& orientation, int facings) const;

  WAngle QuantizeFacing(WAngle facing, int facings) const;
};

class BodyOrientation final : public TraitBase, public ISync {
 public:
  BodyOrientation(ActorInitializer& init,
                  const BodyOrientationInfoData& info);

  ORA_TRAIT_INTERFACES(BodyOrientation,
                       OpenRA_Mods_Common_Traits_BodyOrientation, ISync)

  int QuantizedFacings() const;

  WAngle CameraPitch() const { return info_.angle_camera_pitch; }

  WVec LocalToWorld(const WVec& vec) const {
    return info_.LocalToWorld(vec);
  }

  WRot QuantizeOrientation(const WRot& orientation) const;

  WAngle QuantizeFacing(WAngle facing) const;

  WAngle QuantizeFacing(WAngle facing, int facings) const {
    return info_.QuantizeFacing(facing, facings);
  }

 private:
  BodyOrientationInfoData info_;
  Actor* p_self_ = nullptr;

  std::string str_faction_;

  mutable int int4_quantized_facings_ = 0;
  mutable bool b_quantized_facings_resolved_ = false;

};

void SetBodyOrientationFacingsResolver(
    std::function<int(const game::ActorInfo&, const std::string&)>
        fn_resolve);

int ResolvePreviewQuantizedBodyFacings(const game::ActorInfo& actor,
                                       const std::string& str_faction);

}
