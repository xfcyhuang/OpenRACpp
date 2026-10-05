// UPSTREAM: OpenRA.Mods.Cnc/FileFormats/VxlReader.cs @b6fc03f(全文)。
// Westwood VXL 体素:16 字节头("Voxel Animation" 前缀)+ 尺寸域(LimbCount/
// bodySize)+ 770 保留;每肢体 28 字节头(Name 16 + 12 保留);肢体足注位于
// 802 + 28*LimbCount + bodySize(limbDataOffset u32 + 8 保留 + Scale f32 +
// 48 保留 + Bounds 6×f32 + Size 3 字节 + NormalType 1 字节);体素数据按
// limbDataOffset 回跳,每列 = baseSize 个 i32 列偏移 + 等长跳过表,列内
// do-while 游程(skip 字节 + count 字节 + count×(color,normal) + 重复
// count)。VoxelMap = Dictionary<byte,VxlElement>[x,y](空列 = -1)。
// 形态适配:Stream → SpanReader;Dictionary[,] → 行主序 vector(空 = 无
// 该列;插入序 = 上游 Add 序 = z 升序)。byte z 的模 256 回绕照抄。
// [UPSTREAM continued] VxlReader.cs in full. The Westwood VXL voxel format:
// a 16-byte header (the "Voxel Animation" prefix) + size fields (LimbCount/
// bodySize) + 770 reserved bytes; per-limb 28-byte headers (Name 16 + 12
// reserved); the limb footers at 802 + 28*LimbCount + bodySize (a
// limbDataOffset u32 + 8 reserved + Scale f32 + 48 reserved + Bounds 6×f32 +
// Size 3 bytes + NormalType 1 byte); voxel data reached by limbDataOffset
// seek-back, each column = baseSize i32 column offsets + an equal-length
// skip table, a column's body a do-while run list (skip byte + count byte +
// count×(color,normal) + a duplicate count). VoxelMap =
// Dictionary<byte,VxlElement>[x,y] (an empty column = -1). Shape
// adaptation: Stream → SpanReader; Dictionary[,] → a row-major vector
// (empty = no column; insertion order = upstream's Add order = ascending z).
// The byte z's mod-256 wrap kept.
#pragma once
import std;

#include "formats/span_reader.hpp"

namespace ora::fmt {

/// VxlReader.cs L18:法线类型(值 = 文件里的原始字节)。
/// VxlReader.cs L18: the normal type (the raw byte from the file).
enum class NormalType : std::uint8_t {
  TiberianSun = 2,
  RedAlert2 = 4,
};

/// VxlReader.cs L19 的 readonly record struct。
/// VxlReader.cs L19's readonly record struct.
struct VxlElement {
  std::uint8_t uint1_color;
  std::uint8_t uint1_normal;
};

/// VxlReader.cs L21-31;VoxelMap 为行主序 [x + y*Size[0]]:"存在"标志 = 上
/// 游非 null 槽(colStart ≠ -1;可为空 Dictionary —— count 0 游程列),
/// vec_column_present=false = 上游 null 槽;非空槽的 pair 序 = 上游
/// Dictionary 的 Add 序。
/// VxlReader.cs L21-31; VoxelMap row-major [x + y*Size[0]]: the presence
/// flag = upstream's non-null slot (colStart ≠ -1; may be an empty
/// Dictionary — a count-0 run column), vec_column_present=false = upstream's
/// null slot; a filled slot's pair order = upstream Dictionary's Add order.
struct VxlLimb {
  std::string str_name;
  float fp4_scale = 0.0f;
  std::array<float, 6> arr_bounds{};
  std::array<std::uint8_t, 3> arr_size{};
  NormalType enum_type{};

  std::uint32_t uint4_voxel_count = 0;
  std::vector<std::vector<std::pair<std::uint8_t, VxlElement>>> vec_voxel_map;
  std::vector<std::uint8_t> vec_column_present;
};

/// VxlReader.cs L33-157(全文逐语义)。构造失败抛 InvalidDataException 的
/// 等价("Invalid vxl header" 逐字;其余坏数据 = SpanReader 越界抛点)。
/// VxlReader.cs L33-157 (verbatim semantics). A failed construction throws
/// the InvalidDataException equivalent ("Invalid vxl header" verbatim; other
/// malformed data hits SpanReader's bounds throws).
class VxlReader {
 public:
  explicit VxlReader(std::span<const std::byte> vec_file);

  std::uint32_t uint4_limb_count = 0;
  std::vector<VxlLimb> vec_limbs;
};

/// 上游 StreamExts.ReadASCII(StreamExts.cs L149-153):n 字节按
/// Encoding.ASCII 解码(>0x7F 置 '?')。
/// Upstream StreamExts.ReadASCII (StreamExts.cs L149-153): n bytes decoded
/// per Encoding.ASCII (anything >0x7F becomes '?').
std::string ReadAscii(std::span<const std::byte> vec_bytes);

}  // namespace ora::fmt
