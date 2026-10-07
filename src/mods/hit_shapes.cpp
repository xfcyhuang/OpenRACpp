// UPSTREAM: OpenRA.Mods.Common/HitShapes/IHitShape.cs + Circle.cs +
//          Capsule.cs + Polygon.cs + Rectangle.cs 实现部分
//          The implementation half.
#include "mods/hit_shapes.hpp"

#include "core/exts_math.hpp"
#include "core/polygon.hpp"
#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::mods {

namespace {

/// int2 字段读取(Int2 槽 = GenericTuple 二分量)
/// The int2 field read (an Int2 slot = a two-component GenericTuple).
std::optional<int2> RecordFieldInt2(const meta::RecordObject& rec,
                                    std::string_view str_name) {
  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++)
      if (fields[i]->str_name == str_name) {
        const meta::GenericValue& v = generated->Slot(i);
        if (auto* tuple = std::get_if<meta::GenericTuple>(&v.val))
          return int2{static_cast<int>(tuple->arr_ints[0]),
                      static_cast<int>(tuple->arr_ints[1])};
      }
  }
  return std::nullopt;
}

std::optional<std::int64_t> RecordFieldIntOf(const meta::RecordObject& rec,
                                             std::string_view str_name) {
  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++)
      if (fields[i]->str_name == str_name) {
        const meta::GenericValue& v = generated->Slot(i);
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          return *n;
      }
  }
  return std::nullopt;
}

std::optional<std::vector<int2>> RecordFieldInt2Array(
    const meta::RecordObject& rec, std::string_view str_name) {
  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++)
      if (fields[i]->str_name == str_name) {
        const meta::GenericValue& v = generated->Slot(i);
        if (auto* list = std::get_if<std::vector<meta::GenericValue>>(&v.val)) {
          std::vector<int2> vec_out;
          for (const auto& element : *list)
            if (auto* tuple = std::get_if<meta::GenericTuple>(&element.val))
              vec_out.push_back(
                  int2{static_cast<int>(tuple->arr_ints[0]),
                       static_cast<int>(tuple->arr_ints[1])});
          return vec_out;
        }
      }
  }
  return std::nullopt;
}

std::optional<WAngle> RecordFieldWAngle(const meta::RecordObject& rec,
                                        std::string_view str_name) {
  const auto v = RecordFieldIntOf(rec, str_name);
  if (v)
    return WAngle{static_cast<std::int32_t>(*v)};
  return std::nullopt;
}

}  // namespace

// ———— CircleShape ————

void CircleShape::Initialize() {
  // L38-42
  if (VerticalTopOffset < VerticalBottomOffset)
    throw yaml::YamlException(
        "VerticalTopOffset must be equal to or higher than "
        "VerticalBottomOffset.");
}

WDist CircleShape::DistanceFromEdge(const WVec& v) const {
  // L44-46
  return WDist{std::max(0, v.Length() - Radius.Length)};
}

WDist CircleShape::DistanceFromEdge(const WPos& pos, const WPos& origin,
                                    const WRot& /*orientation*/) const {
  // L48-60
  if (pos.Z > origin.Z + VerticalTopOffset)
    return DistanceFromEdge(pos - (origin + WVec{0, 0, VerticalTopOffset}));

  if (pos.Z < origin.Z + VerticalBottomOffset)
    return DistanceFromEdge(
        pos - (origin + WVec{0, 0, VerticalBottomOffset}));

  return DistanceFromEdge(
      pos - WPos{origin.X, origin.Y, pos.Z});
}

// ———— CapsuleShape ————

void CapsuleShape::Initialize() {
  // L51-63
  ab_ = PointB - PointA;
  ab_len_sq_ = ab_.LengthSquared() / 1024;

  if (ab_len_sq_ == 0)
    throw yaml::YamlException(
        "This Capsule describes a circle. Use a Circle HitShape instead.");

  if (VerticalTopOffset < VerticalBottomOffset)
    throw yaml::YamlException(
        "VerticalTopOffset must be equal to or higher than "
        "VerticalBottomOffset.");

  outer_radius_ =
      Radius + WDist{std::max(PointA.Length(), PointB.Length())};
}

WDist CapsuleShape::DistanceFromEdge(const WVec& v) const {
  // L65-83
  const int2 p{v.X, v.Y};

  const int t = int2::Dot(p - PointA, ab_) / ab_len_sq_;

  if (t < 0)
    return WDist{std::max(0, (PointA - p).Length() - Radius.Length)};
  if (t > 1024)
    return WDist{std::max(0, (PointB - p).Length() - Radius.Length)};

  const int2 projection =
      PointA + int2{ab_.X * t / 1024, ab_.Y * t / 1024};

  const int distance = (projection - p).Length();

  return WDist{std::max(0, distance - Radius.Length)};
}

WDist CapsuleShape::DistanceFromEdge(const WPos& pos, const WPos& origin,
                                     const WRot& orientation) const {
  // L85-94
  if (pos.Z > origin.Z + VerticalTopOffset)
    return DistanceFromEdge(
        (pos - (origin + WVec{0, 0, VerticalTopOffset}))
            .Rotate(-orientation));

  if (pos.Z < origin.Z + VerticalBottomOffset)
    return DistanceFromEdge(
        (pos - (origin + WVec{0, 0, VerticalBottomOffset}))
            .Rotate(-orientation));

  return DistanceFromEdge(
      (pos - WPos{origin.X, origin.Y, pos.Z}).Rotate(-orientation));
}

// ———— PolygonShape ————

void PolygonShape::Initialize() {
  // L48-60
  if (VerticalTopOffset < VerticalBottomOffset)
    throw yaml::YamlException(
        "VerticalTopOffset must be equal to or higher than "
        "VerticalBottomOffset.");

  int max_length = 0;
  for (const int2& p : Points)
    max_length = std::max(max_length, p.Length());
  outer_radius_ = WDist{max_length};
  vec_combat_overlay_verts_top_.clear();
  vec_combat_overlay_verts_bottom_.clear();
  for (const int2& p : Points) {
    vec_combat_overlay_verts_top_.push_back(
        WVec{p.X, p.Y, VerticalTopOffset});
    vec_combat_overlay_verts_bottom_.push_back(
        WVec{p.X, p.Y, VerticalBottomOffset});
  }
  vec_squares_.assign(Points.size(), 0);
  vec_squares_[0] = (Points[0] - Points[Points.size() - 1]).LengthSquared();
  for (std::size_t i = 1; i < Points.size(); i++)
    vec_squares_[i] = (Points[i] - Points[i - 1]).LengthSquared();
}

int PolygonShape::DistanceSquaredFromLineSegment(int2 c, int2 a, int2 b,
                                                 int ab2) {
  // L62-84
  const int2 ac = c - a;
  const int ac2 = ac.LengthSquared();
  const int bc2 = (c - b).LengthSquared();

  // c is closest to point a
  if (ac2 + ab2 <= bc2)
    return ac2;

  // c is closest to point b
  if (bc2 + ab2 <= ac2)
    return bc2;

  // c is closest to its unknown orthogonal projection (p) onto the line
  // spanned by b with a as the origin(上游注释;long 防溢出照抄)
  // (the upstream comment; the long overflow guard kept).
  const int2 ab = b - a;
  const int ap2 = ac.X * ab.X + ac.Y * ab.Y;
  const int2 ap{
      static_cast<int>(static_cast<std::int64_t>(ab.X) * ap2 / ab2),
      static_cast<int>(static_cast<std::int64_t>(ab.Y) * ap2 / ab2)};

  // Length of vector pc squared.
  return (ac - ap).LengthSquared();
}

WDist PolygonShape::DistanceFromEdge(const WVec& v) const {
  // L86-102
  const int2 p{v.X, v.Y};
  const int z = std::abs(v.Z);
  if (PolygonContains(std::span<const int2>{Points}, p))
    return WDist{z};

  int min2 = DistanceSquaredFromLineSegment(
      p, Points[Points.size() - 1], Points[0], vec_squares_[0]);
  for (std::size_t i = 1; i < Points.size(); i++) {
    const int d2 =
        DistanceSquaredFromLineSegment(p, Points[i - 1], Points[i],
                                       vec_squares_[i]);
    if (d2 < min2)
      min2 = d2;
  }

  return WDist{static_cast<std::int32_t>(ISqrt(static_cast<std::uint64_t>(min2) + static_cast<std::uint64_t>(z) * z))};
}

WDist PolygonShape::DistanceFromEdge(const WPos& pos, const WPos& origin,
                                     const WRot& orientation) const {
  // L104-115
  const WRot rotated = orientation + WRot::FromYaw(LocalYaw);

  if (pos.Z > origin.Z + VerticalTopOffset)
    return DistanceFromEdge(
        (pos - (origin + WVec{0, 0, VerticalTopOffset}))
            .Rotate(-rotated));

  if (pos.Z < origin.Z + VerticalBottomOffset)
    return DistanceFromEdge(
        (pos - (origin + WVec{0, 0, VerticalBottomOffset}))
            .Rotate(-rotated));

  return DistanceFromEdge(
      (pos - WPos{origin.X, origin.Y, pos.Z}).Rotate(-rotated));
}

// ———— RectangleShape ————

void RectangleShape::Initialize() {
  // L58-105
  if (TopLeft.X >= BottomRight.X || TopLeft.Y >= BottomRight.Y)
    throw yaml::YamlException("TopLeft and BottomRight points are invalid.");

  if (VerticalTopOffset < VerticalBottomOffset)
    throw yaml::YamlException(
        "VerticalTopOffset must be equal to or higher than "
        "VerticalBottomOffset.");

  quadrant_size_ = int2{(BottomRight.X - TopLeft.X) / 2,
                        (BottomRight.Y - TopLeft.Y) / 2};
  center_ = TopLeft + quadrant_size_;

  const int2 top_right{BottomRight.X, TopLeft.Y};
  const int2 bottom_left{TopLeft.X, BottomRight.Y};
  const int2 corners[] = {TopLeft, BottomRight, top_right, bottom_left};
  int max_length = 0;
  for (const int2& x : corners)
    max_length = std::max(max_length, x.Length());
  outer_radius_ = WDist{max_length};
}

WDist RectangleShape::DistanceFromEdge(const WVec& v) const {
  // L107-114
  const WVec r{std::max(std::abs(v.X - center_.X) - quadrant_size_.X, 0),
               std::max(std::abs(v.Y - center_.Y) - quadrant_size_.Y, 0),
               0};

  return WDist{r.HorizontalLength()};
}

WDist RectangleShape::DistanceFromEdge(const WPos& pos, const WPos& origin,
                                       const WRot& orientation) const {
  // L116-127
  const WRot rotated = orientation + WRot::FromYaw(LocalYaw);

  if (pos.Z > origin.Z + VerticalTopOffset)
    return DistanceFromEdge(
        (pos - (origin + WVec{0, 0, VerticalTopOffset}))
            .Rotate(-rotated));

  if (pos.Z < origin.Z + VerticalBottomOffset)
    return DistanceFromEdge(
        (pos - (origin + WVec{0, 0, VerticalBottomOffset}))
            .Rotate(-rotated));

  return DistanceFromEdge(
      (pos - WPos{origin.X, origin.Y, pos.Z}).Rotate(-rotated));
}

// ———— LoadShape 产物解析 ————

std::unique_ptr<IHitShape> ParseHitShape(const meta::RecordObject& rec_type) {
  const std::string_view str_full = rec_type.record_desc().str_full_name;

  if (str_full == "OpenRA.Mods.Common.HitShapes.CircleShape") {
    auto shape = std::make_unique<CircleShape>();
    if (const auto v = RecordFieldIntOf(rec_type, "Radius"))
      shape->Radius = WDist{static_cast<std::int32_t>(*v)};
    if (const auto v = RecordFieldIntOf(rec_type, "VerticalTopOffset"))
      shape->VerticalTopOffset = static_cast<int>(*v);
    if (const auto v = RecordFieldIntOf(rec_type, "VerticalBottomOffset"))
      shape->VerticalBottomOffset = static_cast<int>(*v);
    shape->Initialize();
    return shape;
  }

  if (str_full == "OpenRA.Mods.Common.HitShapes.CapsuleShape") {
    auto shape = std::make_unique<CapsuleShape>();
    if (const auto v = RecordFieldInt2(rec_type, "PointA"))
      shape->PointA = *v;
    if (const auto v = RecordFieldInt2(rec_type, "PointB"))
      shape->PointB = *v;
    if (const auto v = RecordFieldIntOf(rec_type, "Radius"))
      shape->Radius = WDist{static_cast<std::int32_t>(*v)};
    if (const auto v = RecordFieldIntOf(rec_type, "VerticalTopOffset"))
      shape->VerticalTopOffset = static_cast<int>(*v);
    if (const auto v = RecordFieldIntOf(rec_type, "VerticalBottomOffset"))
      shape->VerticalBottomOffset = static_cast<int>(*v);
    shape->Initialize();
    return shape;
  }

  if (str_full == "OpenRA.Mods.Common.HitShapes.PolygonShape") {
    auto shape = std::make_unique<PolygonShape>();
    if (const auto v = RecordFieldInt2Array(rec_type, "Points"))
      shape->Points = std::move(*v);
    if (const auto v = RecordFieldIntOf(rec_type, "VerticalTopOffset"))
      shape->VerticalTopOffset = static_cast<int>(*v);
    if (const auto v = RecordFieldIntOf(rec_type, "VerticalBottomOffset"))
      shape->VerticalBottomOffset = static_cast<int>(*v);
    if (const auto v = RecordFieldWAngle(rec_type, "LocalYaw"))
      shape->LocalYaw = *v;
    shape->Initialize();
    return shape;
  }

  if (str_full == "OpenRA.Mods.Common.HitShapes.RectangleShape") {
    auto shape = std::make_unique<RectangleShape>();
    if (const auto v = RecordFieldInt2(rec_type, "TopLeft"))
      shape->TopLeft = *v;
    if (const auto v = RecordFieldInt2(rec_type, "BottomRight"))
      shape->BottomRight = *v;
    if (const auto v = RecordFieldIntOf(rec_type, "VerticalTopOffset"))
      shape->VerticalTopOffset = static_cast<int>(*v);
    if (const auto v = RecordFieldIntOf(rec_type, "VerticalBottomOffset"))
      shape->VerticalBottomOffset = static_cast<int>(*v);
    if (const auto v = RecordFieldWAngle(rec_type, "LocalYaw"))
      shape->LocalYaw = *v;
    shape->Initialize();
    return shape;
  }

  return nullptr;
}

}  // namespace ora::mods
