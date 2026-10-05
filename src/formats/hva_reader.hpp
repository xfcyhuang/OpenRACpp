// UPSTREAM: OpenRA.Mods.Cnc/FileFormats/HvaReader.cs @b6fc03f(全文)+
// OpenRA.Mods.Cnc/Util.cs L131-255 的 MatrixInverse 子集(HVA 唯一消费面;
// Util 余部随 ModelRenderer 批次移植)。
// Westwood HVA 骨骼动画:16 字节名 + FrameCount/LimbCount u32 + 16×LimbCount
// 肢体名跳过 + 每帧每肢体 12 个 f32 —— 经转置表 ids 写入 16 元列主序矩阵
// (末行/列 0,0,0,1),并逐矩阵 MatrixInverse 可逆性校验(det == 0 抛)。
// 形态适配:Stream → SpanReader;MatrixInverse 返回 null → std::optional。
// [UPSTREAM continued] HvaReader.cs in full plus the MatrixInverse subset of
// Util.cs L131-255 (HVA's only consumer; the rest of Util lands with the
// ModelRenderer batch). The Westwood HVA skeletal animation: a 16-byte name +
// FrameCount/LimbCount u32s + a 16×LimbCount limb-name skip + 12 f32 per
// frame per limb — written through the transpose table ids into 16-element
// column-major matrices (the final row/column 0,0,0,1), each validated for
// invertibility by MatrixInverse (det == 0 throws). Shape adaptation:
// Stream → SpanReader; MatrixInverse's null return → std::optional.
#pragma once
import std;

#include "formats/span_reader.hpp"

namespace ora::fmt {

/// HvaReader.cs L17-62(全文逐语义)。Transforms 为 16×FrameCount×LimbCount
/// 的列主序矩阵序列(帧 j 肢体 i 的基址 = 16*(LimbCount*j + i))。
/// 抛点:不可逆矩阵的 InvalidDataException 等价(消息逐字,含文件名/节
/// 号/帧号)。
/// HvaReader.cs L17-62 (verbatim semantics). Transforms holds
/// 16×FrameCount×LimbCount column-major matrices (frame j limb i at
/// 16*(LimbCount*j + i)). Throw point: the non-invertible matrix's
/// InvalidDataException equivalent (message verbatim, with the file name/
/// section/frame numbers).
class HvaReader {
 public:
  HvaReader(std::span<const std::byte> vec_file, std::string_view str_file_name);

  std::uint32_t uint4_frame_count = 0;
  std::uint32_t uint4_limb_count = 0;
  std::vector<float> vec_transforms;
};

/// Util.MatrixInverse(Util.cs L131-255)的逐语义子集:伴随矩阵法 + 行列式
/// 除法;det == 0 返回 nullopt(上游 null)。float 运算序逐字照抄。
/// The verbatim-semantics subset of Util.MatrixInverse (Util.cs L131-255):
/// the adjugate route + division by the determinant; det == 0 returns
/// nullopt (upstream null). The float operation order copied verbatim.
std::optional<std::array<float, 16>> MatrixInverse(const std::array<float, 16>& arr_m);

}  // namespace ora::fmt
