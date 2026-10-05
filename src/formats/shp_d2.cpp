// UPSTREAM: OpenRA.Mods.Cnc/SpriteLoaders/ShpD2Loader.cs @7d57605 L17-172
// 实现文件:ShpD2Frame 头解析/查表/LCW 预解压/RLE0、IsShpD2、ParseFrames
// 逐句照抄;Stream → SpanReader。
// Implementation file: the ShpD2Frame header parse / lookup table / LCW
// pre-decode / RLE0, IsShpD2, and ParseFrames copied statement by
// statement; Stream → SpanReader.
#include "formats/shp_d2.hpp"

#include "formats/lcw.hpp"
#include "formats/rle_zeros.hpp"
#include "formats/span_reader.hpp"

namespace ora::fmt {

namespace {

/// 上游 [Flags] enum FormatFlags : ushort(L21-27)。
/// The upstream [Flags] enum FormatFlags : ushort (L21-27).
enum class FormatFlags : std::uint16_t {
  PaletteTable = 1,
  NotLCWCompressed = 2,
  VariableLengthTable = 4,
};

constexpr FormatFlags operator&(FormatFlags v_a, FormatFlags v_b) {
  return static_cast<FormatFlags>(static_cast<std::uint16_t>(v_a) & static_cast<std::uint16_t>(v_b));
}

/// ShpD2Frame(L29-90):单帧头 + 查表解压。
/// ShpD2Frame (L29-90): a single frame header + table-lookup decode.
class ShpD2Frame final : public gfx::ISpriteFrame {
 public:
  explicit ShpD2Frame(SpanReader& stream) {
    const auto uint2_flags = stream.ReadUInt16();
    stream.Skip(1);
    const auto uint2_width = stream.ReadUInt16();
    const auto uint1_height = stream.ReadUInt8();
    int2_size_ = int2{uint2_width, uint1_height};

    // 减去头尺寸 | subtract the header size
    auto int4_data_left = static_cast<std::int32_t>(stream.ReadUInt16()) - 10;
    const auto int4_data_size = static_cast<std::int32_t>(stream.ReadUInt16());

    auto vec_table = std::vector<std::byte>{};
    if ((static_cast<FormatFlags>(uint2_flags) & FormatFlags::PaletteTable) != FormatFlags{0}) {
      const auto uint1_n =
          (static_cast<FormatFlags>(uint2_flags) & FormatFlags::VariableLengthTable) != FormatFlags{0}
              ? stream.ReadUInt8()
              : std::uint8_t{16};
      vec_table.resize(uint1_n);
      for (auto int4_i = 0; int4_i < uint1_n; int4_i++)
        vec_table[static_cast<std::size_t>(int4_i)] = static_cast<std::byte>(stream.ReadUInt8());

      // dataLeft 按上游 int 语义推进(负值仅坏数据可达;SpanReader 的
      // 等价抛点见 ReadBytes)。
      // dataLeft advances with the upstream int semantics (negative only
      // on malformed data; see ReadBytes for SpanReader's equivalent
      // throw).
      int4_data_left -= uint1_n;
    } else {
      vec_table.resize(256);
      for (auto int4_i = 0; int4_i < 256; int4_i++)
        vec_table[static_cast<std::size_t>(int4_i)] = static_cast<std::byte>(int4_i);
      vec_table[1] = static_cast<std::byte>(0x7F);
      vec_table[2] = static_cast<std::byte>(0x7E);
      vec_table[3] = static_cast<std::byte>(0x7D);
      vec_table[4] = static_cast<std::byte>(0x7C);
    }

    vec_data_.resize(static_cast<std::size_t>(uint2_width) * static_cast<std::size_t>(uint1_height));

    // 解码图像数据 | decode the image data
    auto vec_compressed = std::vector<std::byte>{};
    vec_compressed.assign_range(stream.ReadBytes(int4_data_left));
    if ((static_cast<FormatFlags>(uint2_flags) & FormatFlags::NotLCWCompressed) == FormatFlags{0}) {
      auto vec_temp = std::vector<std::byte>(static_cast<std::size_t>(int4_data_size));
      lcw::DecodeInto(vec_compressed, vec_temp);
      vec_compressed = std::move(vec_temp);
    }

    rle_zeros::DecodeInto(vec_compressed, vec_data_, 0);

    // 查表回填 | look up values in the lookup table
    for (auto& byte_data : vec_data_)
      byte_data = vec_table[static_cast<std::uint8_t>(byte_data)];
  }

  gfx::SpriteFrameType Type() const override { return gfx::SpriteFrameType::Indexed8; }
  int2 Size() const override { return int2_size_; }
  int2 FrameSize() const override { return Size(); }
  core::Vector2 Offset() const override { return core::Vector2{}; }
  std::span<const std::byte> Data() const override { return vec_data_; }
  bool DisableExportPadding() const override { return false; }

 private:
  int2 int2_size_{};
  std::vector<std::byte> vec_data_;
};

}  // namespace

bool IsShpD2(std::span<const std::byte> vec_file) {
  auto stream = SpanReader{vec_file};
  const auto int8_start = stream.Position();

  // 首个 word 是帧数 | the first word is the image count
  const auto uint2_image_count = stream.ReadUInt16();
  if (uint2_image_count == 0) {
    stream.Seek(int8_start);
    return false;
  }

  // 测试偏移是 2 字节还是 4 字节 | test for two vs four byte offsets
  const auto uint4_test_offset = stream.ReadUInt32();
  const auto int4_offset_size = (uint4_test_offset & 0xFF0000) > 0 ? 2 : 4;

  // 末偏移应指向文件尾 | the last offset should point to the end of file
  const auto int8_final_offset = int8_start + 2 + int4_offset_size * static_cast<std::int64_t>(uint2_image_count);
  if (int8_final_offset > stream.Length()) {
    stream.Seek(int8_start);
    return false;
  }

  stream.Seek(int8_final_offset);
  const auto int8_eof = int4_offset_size == 2 ? static_cast<std::int64_t>(stream.ReadUInt16())
                                              : static_cast<std::int64_t>(stream.ReadUInt32());
  if (int8_eof + 2 != stream.Length()) {
    stream.Seek(int8_start);
    return false;
  }

  // 检查首帧格式标志 | check the format flag on the first frame
  const auto uint2_b = stream.ReadUInt16();
  stream.Seek(int8_start);
  return uint2_b == 5 || uint2_b <= 3;
}

namespace {

/// ParseFrames(L121-151)。
std::vector<std::unique_ptr<gfx::ISpriteFrame>> ParseFrames(std::span<const std::byte> vec_file) {
  auto stream = SpanReader{vec_file};
  const auto uint2_image_count = stream.ReadUInt16();

  // 末偏移指向文件尾。| the last offset is a pointer to the end of file
  auto arr_offsets = std::vector<std::uint32_t>(uint2_image_count + 1);
  const auto uint4_temp = stream.ReadUInt32();

  // 文件第四字节非零则偏移为 2 字节型。| if the fourth byte in the file
  // is non-zero, the offsets are two bytes each.
  const auto b_two_byte_offset = (uint4_temp & 0xFF0000) > 0;
  stream.Seek(2);

  for (std::size_t st_i = 0; st_i < arr_offsets.size(); st_i++)
    arr_offsets[st_i] = (b_two_byte_offset ? stream.ReadUInt16() : stream.ReadUInt32()) + 2;

  auto vec_frames = std::vector<std::unique_ptr<gfx::ISpriteFrame>>{};
  vec_frames.reserve(uint2_image_count);
  for (auto int4_i = 0; int4_i < uint2_image_count; int4_i++) {
    stream.Seek(arr_offsets[static_cast<std::size_t>(int4_i)]);
    vec_frames.push_back(std::make_unique<ShpD2Frame>(stream));
  }

  return vec_frames;
}

}  // namespace

bool TryParseShpD2(std::span<const std::byte> vec_file, std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames) {
  if (!IsShpD2(vec_file))
    return false;

  vec_frames = ParseFrames(vec_file);
  return true;
}

}  // namespace ora::fmt
