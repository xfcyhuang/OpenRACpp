#include "mods/body_orientation.hpp"

#include "game/actor_info.hpp"
#include "meta/generic_record.hpp"
#include "mods/util.hpp"
#include "sim/trait_registry.hpp"

namespace ora::mods {

BodyOrientationInfoData BodyOrientationInfoData::Parse(
    const meta::RecordObject& rec_info) {
  BodyOrientationInfoData data;

  if (const auto v = sim::RecordFieldInt(rec_info, "QuantizedFacings"))
    data.int4_quantized_facings = static_cast<int>(*v);
  if (const auto v = sim::RecordFieldInt(rec_info, "CameraPitch"))
    data.angle_camera_pitch = WAngle{static_cast<std::int32_t>(*v)};
  if (const auto v = sim::RecordFieldInt(rec_info,
                                         "UseClassicPerspectiveFudge"))
    data.b_use_classic_perspective_fudge = *v != 0;

  return data;
}

WVec BodyOrientationInfoData::LocalToWorld(const WVec& vec) const {

  if (!b_use_classic_perspective_fudge)
    return WVec{vec.Y, -vec.X, vec.Z};

  return WVec{vec.Y, -angle_camera_pitch.Sin() * vec.X / 1024, vec.Z};
}

WRot BodyOrientationInfoData::QuantizeOrientation(const WRot& orientation,
                                                  int facings) const {

  if (facings == 0)
    return orientation;

  const WAngle facing = QuantizeFacing(orientation.Yaw, facings);

  return WRot::FromYaw(facing);
}

WAngle BodyOrientationInfoData::QuantizeFacing(WAngle facing,
                                               int facings) const {

  if (facings == 0)
    return facing;

  return ora::mods::QuantizeFacing(facing, facings);
}

namespace {

std::function<int(const game::ActorInfo&, const std::string&)>
    fn_facings_resolver_ = nullptr;
}

void SetBodyOrientationFacingsResolver(
    std::function<int(const game::ActorInfo&, const std::string&)>
        fn_resolve) {
  fn_facings_resolver_ = std::move(fn_resolve);
}

BodyOrientation::BodyOrientation(ActorInitializer& init,
                                 const BodyOrientationInfoData& info)
    : info_(info),
      p_self_(&init.Self()) {

  str_faction_ = init.GetValue<sim::FactionInit, std::string>(
      p_self_->Owner() != nullptr
          ? p_self_->Owner()->Faction().InternalName
          : std::string());
}

int BodyOrientation::QuantizedFacings() const {

  if (b_quantized_facings_resolved_)
    return int4_quantized_facings_;

  Actor& self = *p_self_;

  if (info_.int4_quantized_facings >= 0) {
    int4_quantized_facings_ = info_.int4_quantized_facings;
    b_quantized_facings_resolved_ = true;
    return int4_quantized_facings_;
  }

  const bool has_qboi =
      self.Info() != nullptr &&
      sim::FindTraitInfoOfInterface(
          *self.Info(),
          "OpenRA.Mods.Common.Traits.IQuantizeBodyOrientationInfo") != nullptr;
  if (!has_qboi) {
    if (self.Info() != nullptr &&
        self.Info()->HasTraitInfoOfInterface(
            "OpenRA.Mods.Common.Traits.Render.WithSpriteBodyInfo"))
      throw std::runtime_error(
          "Actor '" + self.InfoName() +
          "' has a sprite body but no facing quantization."
          " Either add the QuantizeFacingsFromSequence trait or set custom "
          "QuantizedFacings on BodyOrientation.");
    throw std::runtime_error(
        "Actor type '" + self.InfoName() +
        "' does not define a quantized body orientation.");
  }

  const std::string str_qboi_name = "QuantizeFacingsFromSequenceInfo";
  const int facings =
      fn_facings_resolver_ != nullptr && self.Info() != nullptr
          ? fn_facings_resolver_(*self.Info(), str_faction_)
          : 0;
  if (facings == 0)
    throw std::runtime_error(
        "Actor " + self.InfoName() + " defines a quantized body orientation from " +
        str_qboi_name + " with zero facings.");

  int4_quantized_facings_ = facings;
  b_quantized_facings_resolved_ = true;
  return int4_quantized_facings_;
}

WRot BodyOrientation::QuantizeOrientation(const WRot& orientation) const {

  return info_.QuantizeOrientation(orientation, QuantizedFacings());
}

WAngle BodyOrientation::QuantizeFacing(WAngle facing) const {

  return info_.QuantizeFacing(facing, QuantizedFacings());
}

int ResolvePreviewQuantizedBodyFacings(const game::ActorInfo& actor,
                                       const std::string& str_faction) {
  const bool has_qboi =
      sim::FindTraitInfoOfInterface(
          actor, "OpenRA.Mods.Common.Traits.IQuantizeBodyOrientationInfo") !=
      nullptr;
  if (!has_qboi)
    return 1;

  return fn_facings_resolver_ != nullptr
             ? fn_facings_resolver_(actor, str_faction)
             : 0;
}

}
