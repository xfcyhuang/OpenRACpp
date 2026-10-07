// UPSTREAM: OpenRA.Mods.Common/Traits/BodyOrientation.cs @b6fc03f L18-127
//          全文(逐语义重写)
//          The whole of BodyOrientation.cs L18-127 (verbatim-semantics
//          rewrite).
//
// 机制对照 / Mechanism mapping:
//  - Info 侧(值袋无方法)→ BodyOrientationInfoData 解析值 + 数据方法;
//    LocalToWorld/QuantizeOrientation/QuantizeFacing 的算式随数据对象
//    The Info side (the bag is method-free) → the parsed
//    BodyOrientationInfoData + its data methods; the LocalToWorld/
//    QuantizeOrientation/QuantizeFacing formulas ride the data object.
//  - QuantizedFacings 的 Lazy → 首查物化(同求值时点;IQuantizeBodyOrientation
//    实现批未至时按上游 qboi==null 抛路径,异常文本逐字)
//    The Lazy QuantizedFacings → first-query materialization (the same
//    evaluation moment; until the IQuantizeBodyOrientation implementing
//    batch arrives this follows upstream's qboi==null throw path, with
//    verbatim exception texts).
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

/// BodyOrientationInfo 的解析面(L18-62:三字段 + LocalToWorld/
/// QuantizeOrientation/QuantizeFacing)
/// The parsed face of BodyOrientationInfo (L18-62: the three fields +
/// LocalToWorld/QuantizeOrientation/QuantizeFacing).
struct BodyOrientationInfoData {
  int int4_quantized_facings = -1;   // L21
  WAngle angle_camera_pitch = WAngle::FromDegrees(40);  // L24
  bool b_use_classic_perspective_fudge = true;  // L27

  static BodyOrientationInfoData Parse(const meta::RecordObject& rec_info);

  /// LocalToWorld(L31-43)
  WVec LocalToWorld(const WVec& vec) const;

  /// QuantizeOrientation(L45-55)
  WRot QuantizeOrientation(const WRot& orientation, int facings) const;

  /// QuantizeFacing(L57-62;virtual — 覆写面随子类批)
  /// QuantizeFacing (L57-62; virtual — the override face rides the
  /// subclass batch).
  WAngle QuantizeFacing(WAngle facing, int facings) const;
};

/// BodyOrientation(L65-127;ISync:QuantizedFacings [VerifySync])
class BodyOrientation final : public TraitBase, public ISync {
 public:
  BodyOrientation(ActorInitializer& init,
                  const BodyOrientationInfoData& info);  // L70-93

  ORA_TRAIT_INTERFACES(BodyOrientation,
                       OpenRA_Mods_Common_Traits_BodyOrientation, ISync)

  /// L95([VerifySync];首查物化 —— 上游 Lazy)
  /// L95 ([VerifySync]; first-query materialization — upstream's Lazy).
  int QuantizedFacings() const;

  WAngle CameraPitch() const { return info_.angle_camera_pitch; }  // L97

  WVec LocalToWorld(const WVec& vec) const {   // L99-101
    return info_.LocalToWorld(vec);
  }

  WRot QuantizeOrientation(const WRot& orientation) const;  // L103-105

  WAngle QuantizeFacing(WAngle facing) const;  // L107-109

  WAngle QuantizeFacing(WAngle facing, int facings) const {  // L111-113
    return info_.QuantizeFacing(facing, facings);
  }

 private:
  BodyOrientationInfoData info_;        // L66
  Actor* p_self_ = nullptr;             // Lazy 体捕获的 self
                                        // (the self captured by the Lazy body)
  std::string str_faction_;             // Lazy 体捕获的 faction
                                        // (the faction captured by the Lazy body)
  mutable int int4_quantized_facings_ = 0;
  mutable bool b_quantized_facings_resolved_ = false;  // Lazy 物化标记
                                                       // (the Lazy
                                                       // materialization flag)
};

/// qboi 分支的 facings 解析钩子(IQuantizeBodyOrientationInfo.
/// QuantizedBodyFacings 的序列表依赖面):实现 = 渲染批的
/// QuantizeFacingsFromSequence 等(读 Map.Sequences);钩子由引擎装配层/
/// 测试接线,默认 0 = 上游零 facings 抛路径(文本逐字)—— 渲染批落地时
/// 换接真序列表(COVERAGE 登记)
/// The facings-resolution hook of the qboi branch (the sequence-table
/// dependency of IQuantizeBodyOrientationInfo.QuantizedBodyFacings): the
/// implementors are the render batch's QuantizeFacingsFromSequence & co.
/// (reading Map.Sequences); the hook is wired by the engine assembly /
/// tests, with 0 as the default = upstream's zero-facings throw path
/// (texts verbatim) — swapped onto the real sequence table when the render
/// batch lands (registered in COVERAGE).
void SetBodyOrientationFacingsResolver(
    std::function<int(const game::ActorInfo&, const std::string&)>
        fn_resolve);

}  // namespace ora::mods
