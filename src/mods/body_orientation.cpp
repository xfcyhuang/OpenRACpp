// UPSTREAM: OpenRA.Mods.Common/Traits/BodyOrientation.cs @b6fc03f L18-127
//          实现部分(the implementation half)。
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
  // L31-43
  // Rotate by 90 degrees
  if (!b_use_classic_perspective_fudge)
    return WVec{vec.Y, -vec.X, vec.Z};

  // The 2d perspective of older games with square cells doesn't correspond
  // to an orthonormal 3D coordinate system, so fudge the y axis to make
  // things look good
  return WVec{vec.Y, -angle_camera_pitch.Sin() * vec.X / 1024, vec.Z};
}

WRot BodyOrientationInfoData::QuantizeOrientation(const WRot& orientation,
                                                  int facings) const {
  // L45-55
  // Quantization disabled
  if (facings == 0)
    return orientation;

  // Map yaw to the closest facing
  const WAngle facing = QuantizeFacing(orientation.Yaw, facings);

  // Roll and pitch are always zero if yaw is quantized
  return WRot::FromYaw(facing);
}

WAngle BodyOrientationInfoData::QuantizeFacing(WAngle facing,
                                               int facings) const {
  // L57-62
  // Quantization disabled
  if (facings == 0)
    return facing;

  return ora::mods::QuantizeFacing(facing, facings);
}

namespace {
/// qboi facings 解析钩子(默认 0 = 零 facings 抛路径)
/// The qboi facings-resolution hook (0 by default = the zero-facings throw
/// path).
std::function<int(const game::ActorInfo&, const std::string&)>
    fn_facings_resolver_ = nullptr;
}  // namespace

void SetBodyOrientationFacingsResolver(
    std::function<int(const game::ActorInfo&, const std::string&)>
        fn_resolve) {
  fn_facings_resolver_ = std::move(fn_resolve);
}

BodyOrientation::BodyOrientation(ActorInitializer& init,
                                 const BodyOrientationInfoData& info)
    : info_(info),
      p_self_(&init.Self()) {
  // L70-93:Lazy 的求值体捕获 self/faction —— faction = GetValue<
  // FactionInit, string>(self.Owner.Faction.InternalName)
  // L70-93: the Lazy's evaluation body captures self/faction — faction =
  // GetValue<FactionInit, string>(self.Owner.Faction.InternalName).
  str_faction_ = init.GetValue<sim::FactionInit, std::string>(
      p_self_->Owner() != nullptr
          ? p_self_->Owner()->Faction().InternalName
          : std::string());
}

int BodyOrientation::QuantizedFacings() const {
  // L95:quantizedFacings.Value(首查物化 = Lazy)
  // L95: quantizedFacings.Value (first-query materialization = the Lazy).
  if (b_quantized_facings_resolved_)
    return int4_quantized_facings_;

  Actor& self = *p_self_;

  // L76-91:求值体
  // L76-91: the evaluation body.
  // Override value is set
  if (info_.int4_quantized_facings >= 0) {
    int4_quantized_facings_ = info_.int4_quantized_facings;
    b_quantized_facings_resolved_ = true;
    return int4_quantized_facings_;
  }

  // If a sprite actor has neither custom QuantizedFacings nor a trait
  // implementing IQuantizeBodyOrientationInfo, throw
  // (实现批未至:接口名查询恒空 —— 上游同样抛出,文本逐字)
  // (the implementing batch hasn't arrived: the interface-name query is
  // always empty — upstream throws all the same, texts verbatim).
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

  // IQuantizeBodyOrientationInfo 实现面:facings 经解析钩子(序列表依赖
  // 面;渲染批接线 —— COVERAGE 登记);0 = 上游零 facings 抛(文本逐字,
  // 含 Info 类型名)
  // The IQuantizeBodyOrientationInfo implementor face: the facings go
  // through the resolution hook (the sequence-table dependency; wired by
  // the render batch — registered in COVERAGE); 0 = upstream's
  // zero-facings throw (text verbatim, carrying the Info type name).
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
  // L103-105
  return info_.QuantizeOrientation(orientation, QuantizedFacings());
}

WAngle BodyOrientation::QuantizeFacing(WAngle facing) const {
  // L107-109
  return info_.QuantizeFacing(facing, QuantizedFacings());
}

}  // namespace ora::mods
