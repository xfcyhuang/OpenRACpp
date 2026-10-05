// UPSTREAM: OpenRA.Mods.Cnc/SpriteLoaders/ShpTDLoader.cs @7d57605 L17-331
// 实现文件:IsShpTD / ShpTDSprite 构造(ImageHeader 表 + 引用链 +
// Decompress)/ TrimmedFrame 收边,逐句照抄;Stream → SpanReader。
// Implementation file: IsShpTD / the ShpTDSprite construction (the
// ImageHeader table + reference chain + Decompress) and the
// TrimmedFrame wrapper, copied statement by statement; Stream →
// SpanReader.
#include "formats/shp_td.hpp"

#include "formats/lcw.hpp"
#include "formats/xor_delta.hpp"

namespace ora::fmt {

namespace {

/// 上游 enum Format : ushort(XORPrev=0x20, XORLCW=0x40, LCW=0x80;L67)。
/// The upstream enum Format : ushort (XORPrev=0x20, XORLCW=0x40, LCW=0x80;
/// L67).
enum class ShpTDFormat : std::uint16_t {
  XORPrev = 0x20,
  XORLCW = 0x40,
  LCW = 0x80,
};

}  // namespace

bool IsShpTD(std::span<const std::byte> vec_file) {
  auto stream = SpanReader{vec_file};
  const auto int8_start = stream.Position();

  // 首个 word 是帧数 | the first word is the image count
  const auto uint2_image_count = stream.ReadUInt16();
  if (uint2_image_count == 0) {
    stream.Seek(int8_start);
    return false;
  }

  // 末偏移应指向文件尾 | the last offset should point to the end of file
  const auto int8_final_offset = int8_start + 14 + 8 * static_cast<std::int64_t>(uint2_image_count);
  if (int8_final_offset > stream.Length()) {
    stream.Seek(int8_start);
    return false;
  }

  stream.Seek(int8_final_offset);
  const auto uint4_eof = stream.ReadUInt32();
  if (uint4_eof != static_cast<std::uint32_t>(stream.Length())) {
    stream.Seek(int8_start);
    return false;
  }

  // 检查首帧格式标志 | check the format flag on the first frame
  stream.Seek(int8_start + 17);
  const auto uint1_b = stream.ReadUInt8();

  stream.Seek(int8_start);
  return uint1_b == 0x20 || uint1_b == 0x40 || uint1_b == 0x80;
}

/// 帧头(L146-173):data 低 24 位 = FileOffset、高 8 位 = Format;
/// RefOffset/RefFormat 供 XORLCW 定位兄弟帧。
/// The frame header (L146-173): the low 24 bits of data = FileOffset,
/// the high 8 = Format; RefOffset/RefFormat locate the sibling frame for
/// XORLCW.
struct ShpTDSprite::ImageHeader : gfx::ISpriteFrame {
  std::uint32_t uint4_file_offset = 0;
  ShpTDFormat kind_format{};
  std::uint16_t uint2_ref_offset = 0;
  ShpTDFormat kind_ref_format{};
  ImageHeader* ptr_ref_image = nullptr;
  const ShpTDSprite* ptr_reader = nullptr;
  // 上游 byte[] Data(null = 未解压);shared_ptr 使 TrimmedFrame 的直通
  // 分支与头帧共享同一份不可变数据(上游数组引用语义),且帧数组移出
  // ShpTDSprite 后仍有效。
  // The upstream byte[] Data (null = not yet decompressed); shared_ptr
  // lets TrimmedFrame's pass-through branch share the same immutable data
  // (the upstream array-reference semantics) and keeps the frames valid
  // after moving them out of the ShpTDSprite.
  std::shared_ptr<const std::vector<std::byte>> ptr_data;

  ImageHeader(SpanReader& stream, const ShpTDSprite* ptr_reader) : ptr_reader(ptr_reader) {
    const auto uint4_data = stream.ReadUInt32();
    uint4_file_offset = uint4_data & 0xFFFFFF;
    kind_format = static_cast<ShpTDFormat>(uint4_data >> 24);

    uint2_ref_offset = stream.ReadUInt16();
    kind_ref_format = static_cast<ShpTDFormat>(stream.ReadUInt16());
  }

  gfx::SpriteFrameType Type() const override { return gfx::SpriteFrameType::Indexed8; }
  int2 Size() const override { return ptr_reader->Size(); }
  int2 FrameSize() const override { return ptr_reader->Size(); }
  core::Vector2 Offset() const override { return core::Vector2{}; }
  std::span<const std::byte> Data() const override {
    return ptr_data ? std::span<const std::byte>{*ptr_data} : std::span<const std::byte>{};
  }
  bool DisableExportPadding() const override { return false; }
};

/// TrimmedFrame(L69-138):非零像素包围盒裁剪;偶数行列调整防半像素
/// 偏移;Offset = 0.5×(left+right−w+1, top+bottom−h+1)。上游为
/// ShpTDSprite 的嵌套 sealed class,C++ 同构(私有嵌套)。
/// TrimmedFrame (L69-138): trims to the non-zero-pixel bounding box;
/// adjusts by even rows/columns against half-pixel offsets; Offset =
/// 0.5×(left+right−w+1, top+bottom−h+1). Upstream models it as a nested
/// sealed class of ShpTDSprite; C++ mirrors that (private nesting).
struct ShpTDSprite::TrimmedFrame final : gfx::ISpriteFrame {
 public:
  explicit TrimmedFrame(const ImageHeader& header_h) {
    const auto vec_orig_data = header_h.Data();
    const auto int2_orig_size = header_h.Size();
    auto int4_top = int2_orig_size.Y - 1;
    auto int4_bottom = 0;
    auto int4_left = int2_orig_size.X - 1;
    auto int4_right = 0;

    // 扫描帧数据找含非零像素的最左/最上/最右/最下行/列。
    // Scan the frame data for the left-, top-, right-, bottom-most
    // rows/columns with non-zero pixel data.
    auto st_i = std::size_t{0};
    for (auto int4_y = 0; int4_y < int2_orig_size.Y; int4_y++) {
      for (auto int4_x = 0; int4_x < int2_orig_size.X; int4_x++, st_i++) {
        if (vec_orig_data[st_i] != std::byte{0}) {
          int4_top = std::min(int4_y, int4_top);
          int4_bottom = std::max(int4_y, int4_bottom);
          int4_left = std::min(int4_x, int4_left);
          int4_right = std::max(int4_x, int4_right);
        }
      }
    }

    auto int4_trimmed_width = int4_right - int4_left + 1;
    auto int4_trimmed_height = int4_bottom - int4_top + 1;

    // 必须裁去偶数行列,避免半像素偏移。
    // We must be careful to subtract an even number of rows/columns to
    // avoid sub-pixel offsets.
    if ((int4_trimmed_width - int2_orig_size.X) % 2 != 0) {
      if (int4_left > 0)
        int4_left--;
      else
        int4_right++;

      int4_trimmed_width++;
    }

    if ((int4_trimmed_height - int2_orig_size.Y) % 2 != 0) {
      if (int4_top > 0)
        int4_top--;
      else
        int4_bottom++;

      int4_trimmed_height++;
    }

    if (int4_trimmed_width == int2_orig_size.X && int4_trimmed_height == int2_orig_size.Y) {
      // 无可裁,直接沿用原数据。| Nothing to trim, so use old data directly.
      int2_size_ = header_h.Size();
      int2_frame_size_ = header_h.FrameSize();
      vec_offset_ = header_h.Offset();
      // 无裁剪:Data 与头帧共享同一份不可变数据(shared_ptr;上游数组
      // 引用语义的等价物)。
      // No trim: Data shares the header frame's immutable data (the
      // shared_ptr counterpart of upstream's array reference).
      ptr_shared_data_ = header_h.ptr_data;
    } else if (int4_trimmed_width > 0 && int4_trimmed_height > 0) {
      // 裁剪帧。| Trim the frame.
      vec_data_.resize(static_cast<std::size_t>(int4_trimmed_width) * static_cast<std::size_t>(int4_trimmed_height));
      for (auto int4_y = 0; int4_y < int4_trimmed_height; int4_y++)
        std::ranges::copy(
            vec_orig_data.subspan(static_cast<std::size_t>((int4_y + int4_top) * int2_orig_size.X + int4_left),
                                  static_cast<std::size_t>(int4_trimmed_width)),
            vec_data_.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(int4_y) * static_cast<std::size_t>(int4_trimmed_width)));

      int2_size_ = int2{int4_trimmed_width, int4_trimmed_height};
      int2_frame_size_ = int2_orig_size;
      vec_offset_ = core::Vector2{
          0.5f * static_cast<float>(int4_left + int4_right - int2_orig_size.X + 1),
          0.5f * static_cast<float>(int4_top + int4_bottom - int2_orig_size.Y + 1),
      };

      if (std::fmod(vec_offset_.X, 1.0f) != 0.0f || std::fmod(vec_offset_.Y, 1.0f) != 0.0f)
        throw std::runtime_error("Trimmed frame has non-integer offset.");
    }
  }

  gfx::SpriteFrameType Type() const override { return gfx::SpriteFrameType::Indexed8; }
  int2 Size() const override { return int2_size_; }
  int2 FrameSize() const override { return int2_frame_size_; }
  core::Vector2 Offset() const override { return vec_offset_; }
  std::span<const std::byte> Data() const override {
    if (ptr_shared_data_)
      return std::span<const std::byte>{*ptr_shared_data_};
    return vec_data_;
  }
  bool DisableExportPadding() const override { return false; }

 private:
  std::shared_ptr<const std::vector<std::byte>> ptr_shared_data_;
  int2 int2_size_{};
  int2 int2_frame_size_{};
  core::Vector2 vec_offset_{};
  std::vector<std::byte> vec_data_;  // 空 = 无裁剪直通 | empty = pass-through untrimmed
};

ShpTDSprite::ShpTDSprite(std::span<const std::byte> vec_file) {
  auto stream = SpanReader{vec_file};
  int4_image_count_ = stream.ReadUInt16();
  stream.Skip(4);
  const auto uint2_width = stream.ReadUInt16();
  const auto uint2_height = stream.ReadUInt16();
  int2_size_ = int2{uint2_width, uint2_height};

  stream.Skip(4);
  auto vec_headers = std::vector<std::unique_ptr<ImageHeader>>{};
  vec_headers.reserve(static_cast<std::size_t>(int4_image_count_));
  for (auto int4_i = 0; int4_i < int4_image_count_; int4_i++)
    vec_headers.push_back(std::make_unique<ImageHeader>(stream, this));

  // 跳过 eof 与全零头 | skip the eof and all-zeroes headers
  stream.Skip(16);

  // 上游 ToDictionary(FileOffset → header):重复键抛;合法文件无重复,
  // C++ 以等价抛点复刻(COVERAGE 登记)。
  // Upstream's ToDictionary(FileOffset → header) throws on duplicates;
  // valid files carry none, reproduced as an equivalent throw here
  // (registered in COVERAGE).
  auto map_offsets = std::unordered_map<std::uint32_t, ImageHeader*>{};
  for (auto& ptr_header_unique : vec_headers)
    if (!map_offsets.emplace(ptr_header_unique->uint4_file_offset, ptr_header_unique.get()).second)
      throw std::runtime_error("An item with the same key has already been added.");

  for (auto int4_i = 0; int4_i < int4_image_count_; int4_i++) {
    auto& header_h = *vec_headers[static_cast<std::size_t>(int4_i)];
    if (header_h.kind_format == ShpTDFormat::XORPrev)
      header_h.ptr_ref_image = vec_headers[static_cast<std::size_t>(int4_i - 1)].get();
    else if (header_h.kind_format == ShpTDFormat::XORLCW) {
      const auto iter_found = map_offsets.find(header_h.uint2_ref_offset);
      if (iter_found == map_offsets.end())
        throw std::runtime_error("Reference doesn't point to image data " + std::to_string(header_h.uint4_file_offset) +
                                 "->" + std::to_string(header_h.uint2_ref_offset));
      header_h.ptr_ref_image = iter_found->second;
    }
  }

  int8_shp_bytes_file_offset_ = stream.Position();
  vec_shp_bytes_ = stream.ReadBytes(stream.Length() - int8_shp_bytes_file_offset_);

  for (auto& ptr_header_unique : vec_headers)
    Decompress(*ptr_header_unique);

  vec_frames_.reserve(vec_headers.size());
  for (auto& ptr_header_unique : vec_headers)
    vec_frames_.push_back(std::make_unique<TrimmedFrame>(*ptr_header_unique));
}

void ShpTDSprite::Decompress(ImageHeader& header_h) {
  // 空帧无需额外处理 | no extra work is required for empty frames
  if (header_h.Size().X == 0 || header_h.Size().Y == 0)
    return;

  if (int4_recurse_depth_ > int4_image_count_)
    throw std::runtime_error("Format20/40 headers contain infinite loop");

  switch (header_h.kind_format) {
    case ShpTDFormat::XORPrev:
    case ShpTDFormat::XORLCW: {
      if (!header_h.ptr_ref_image->ptr_data) {
        ++int4_recurse_depth_;
        Decompress(*header_h.ptr_ref_image);
        --int4_recurse_depth_;
      }

      auto vec_image = CopyImageData(*header_h.ptr_ref_image->ptr_data);
      xor_delta::DecodeInto(
          vec_shp_bytes_, vec_image,
          static_cast<std::int32_t>(header_h.uint4_file_offset - int8_shp_bytes_file_offset_));
      header_h.ptr_data = std::shared_ptr<const std::vector<std::byte>>{
          std::make_shared<std::vector<std::byte>>(std::move(vec_image))};
      break;
    }

    case ShpTDFormat::LCW: {
      auto vec_image_bytes = std::make_shared<std::vector<std::byte>>(
          static_cast<std::size_t>(int2_size_.X) * static_cast<std::size_t>(int2_size_.Y));
      lcw::DecodeInto(vec_shp_bytes_, *vec_image_bytes,
                      static_cast<std::int32_t>(header_h.uint4_file_offset - int8_shp_bytes_file_offset_));
      header_h.ptr_data = std::shared_ptr<const std::vector<std::byte>>{std::move(vec_image_bytes)};
      break;
    }

    default:
      throw std::runtime_error("");
  }
}

std::vector<std::byte> ShpTDSprite::CopyImageData(const std::vector<std::byte>& vec_base_image) const {
  auto vec_image_data = std::vector<std::byte>(
      static_cast<std::size_t>(int2_size_.X) * static_cast<std::size_t>(int2_size_.Y));
  std::ranges::copy(std::span<const std::byte>{vec_base_image}.first(vec_image_data.size()),
                    vec_image_data.begin());
  return vec_image_data;
}

bool TryParseShpTD(std::span<const std::byte> vec_file, std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames) {
  if (!IsShpTD(vec_file))
    return false;

  auto sprite = ShpTDSprite{vec_file};
  vec_frames = std::move(sprite.vec_frames_);
  return true;
}

}  // namespace ora::fmt
