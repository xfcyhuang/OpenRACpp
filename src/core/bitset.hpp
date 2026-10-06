// UPSTREAM: OpenRA.Game/Primitives/BitSet.cs @b6fc03f L20-173(全类型逐语义重写)
//          Full-type statement-by-statement rewrite of upstream BitSet.cs.
// 已登记偏离(docs/COVERAGE.md)/ Registered deviations (docs/COVERAGE.md):
//  1. 位图用 uint64_t 承载(C# BigInteger 无界);每标签 64 个不同字符串即 abort
//     (实际 mods 中 TargetableType 约 20 种,不可达),越界即程序错误
//     The bit index is carried in uint64_t (C# uses unbounded BigInteger);
//     the 65th distinct string per tag aborts (unreachable in practice —
//     TargetableType tops out around 20 entries).
//  2. 线程模型:分配器按 magic-static 初始化,加载为单线程(C# 侧加锁仅为
//     防御性;引擎规则加载发生在主线程)
//     Threading: allocators initialize via magic statics; rule loading is
//     single-threaded (the C# lock is defensive; engine rule loading happens
//     on the main thread).
//
// 语义要点 / Semantic notes:
//  - 位按"字符串首次出现序"分配(1,2,4,8,…),GetStrings 按分配序枚举——
//    这决定了 BitSet::ToString() 的输出顺序,是对拍兼容面
//    Bits are allocated in first-appearance order (1,2,4,8,…), and GetStrings
//    enumerates in allocation order — this defines BitSet::ToString() output,
//    which is part of the comparison surface.
//  - 每个 Tag 类型独立命名空间(上游 BitSetAllocator<T> 的静态状态)
//    Each Tag type is an independent namespace (the static state of the
//    upstream BitSetAllocator<T>).
#pragma once
import std;

namespace ora::core {

/// 每 Tag 的字符串→位分配器(BitSet.cs L20-78 BitSetAllocator<T>)
/// Per-tag string→bit allocator (BitSet.cs L20-78 BitSetAllocator<T>).
template <class Tag>
class BitSetAllocator {
 public:
  /// GetBits:缺失字符串即分配新位(BitSet.cs L36-44)
  /// GetBits: missing strings allocate new bits (BitSet.cs L36-44).
  static std::uint64_t GetBits(std::span<const std::string> vec_values) {
    std::uint64_t uint8_bits{0};
    for (const std::string& str_value : vec_values)
      uint8_bits |= BitFor(str_value);
    return uint8_bits;
  }

  /// GetBitsNoAlloc:仅映射已分配位,缺失字符串忽略(BitSet.cs L46-57)
  /// GetBitsNoAlloc: maps existing bits only; missing strings are ignored
  /// (BitSet.cs L46-57).
  static std::uint64_t GetBitsNoAlloc(std::span<const std::string> vec_values) {
    std::uint64_t uint8_bits{0};
    auto& map_index = Index();
    for (const std::string& str_value : vec_values) {
      if (const auto it_find = map_index.find(str_value); it_find != map_index.end())
        uint8_bits |= std::uint64_t{1} << it_find->second;
    }
    return uint8_bits;
  }

  /// GetStrings:按分配序返回置位字符串(BitSet.cs L59-68;Cache 迭代序=插入序)
  /// GetStrings: returns the set strings in allocation order (BitSet.cs L59-68;
  /// the Cache iteration order equals insertion order).
  static std::vector<std::string> GetStrings(std::uint64_t uint8_bits) {
    std::vector<std::string> vec_values;
    const auto& vec_order = Order();
    for (std::size_t int4_i{}; int4_i < vec_order.size(); int4_i++)
      if ((uint8_bits & (std::uint64_t{1} << int4_i)) != 0)
        vec_values.push_back(vec_order[int4_i]);
    return vec_values;
  }

  /// BitsContainString(BitSet.cs L70-77)
  static bool BitsContainString(std::uint64_t uint8_bits, std::string_view sv_value) {
    auto& map_index = Index();
    if (const auto it_find = map_index.find(std::string{sv_value}); it_find != map_index.end())
      return (uint8_bits & (std::uint64_t{1} << it_find->second)) != 0;
    return false;
  }

 private:
  // 分配序表 + 名字→下标(全局状态;首次调用时初始化)
  // Allocation-order table + name→index (global state; initialized on first use).
  static std::vector<std::string>& Order() {
    static std::vector<std::string> vec_order;
    return vec_order;
  }
  static std::unordered_map<std::string, std::size_t>& Index() {
    static std::unordered_map<std::string, std::size_t> map_index;
    return map_index;
  }

  static std::uint64_t BitFor(const std::string& str_value) {
    auto& map_index = Index();
    auto& vec_order = Order();
    if (const auto it_find = map_index.find(str_value); it_find != map_index.end())
      return std::uint64_t{1} << it_find->second;
    const std::size_t int4_next{vec_order.size()};
    // 偏离 1:超过 64 个不同字符串即程序错误(C# BigInteger 无此限)
    // Deviation 1: more than 64 distinct strings is a program error (C#
    // BigInteger has no such limit).
    if (int4_next >= 64)
      std::abort();
    vec_order.push_back(str_value);
    map_index.emplace(str_value, int4_next);
    return std::uint64_t{1} << int4_next;
  }
};

/// BitSet<T>(BitSet.cs L80-172):不可变位集,字符串视图按分配器语义解释
/// BitSet<T> (BitSet.cs L80-172): an immutable bit set whose strings are
/// interpreted through the allocator semantics.
template <class Tag>
class BitSet {
 public:
  BitSet() = default;
  explicit BitSet(std::span<const std::string> vec_values)
      : uint8_bits_{BitSetAllocator<Tag>::GetBits(vec_values)} {}

  /// FromStringsNoAlloc(BitSet.cs L89-92)
  static BitSet FromStringsNoAlloc(std::span<const std::string> vec_values) {
    BitSet set_ret;
    set_ret.uint8_bits_ = BitSetAllocator<Tag>::GetBitsNoAlloc(vec_values);
    return set_ret;
  }

  /// 原始位构造(meta 值袋以原始位承载 BitSet 字段 —— dump 协议注释;
  /// C# 侧为 internal 位构造的值袋等价面)
  /// The raw-bits construction (the meta value bag carries BitSet fields
  /// as raw bits — the dump-protocol note; the bag-value equivalent of
  /// C#'s internal bit ctor).
  static BitSet FromRawBits(std::uint64_t uint8_bits) {
    BitSet set_ret;
    set_ret.uint8_bits_ = uint8_bits;
    return set_ret;
  }

  std::uint64_t RawBits() const { return uint8_bits_; }

  /// ToString():分配序逗号连接(BitSet.cs L94-97)
  /// ToString(): comma-joined in allocation order (BitSet.cs L94-97).
  std::string ToString() const {
    std::string str_ret;
    for (const std::string& str_v : BitSetAllocator<Tag>::GetStrings(uint8_bits_)) {
      if (!str_ret.empty())
        str_ret += ',';
      str_ret += str_v;
    }
    return str_ret;
  }

  /// 迭代分配序字符串(GetEnumerator 语义)
  /// Iterate strings in allocation order (GetEnumerator semantics).
  std::vector<std::string> ToStrings() const {
    return BitSetAllocator<Tag>::GetStrings(uint8_bits_);
  }

  bool IsEmpty() const { return uint8_bits_ == 0; }
  bool IsProperSubsetOf(const BitSet& other) const { return IsSubsetOf(other) && !SetEquals(other); }
  bool IsProperSupersetOf(const BitSet& other) const { return IsSupersetOf(other) && !SetEquals(other); }
  bool IsSubsetOf(const BitSet& other) const { return (uint8_bits_ | other.uint8_bits_) == other.uint8_bits_; }
  bool IsSupersetOf(const BitSet& other) const { return (uint8_bits_ | other.uint8_bits_) == uint8_bits_; }
  bool Overlaps(const BitSet& other) const { return (uint8_bits_ & other.uint8_bits_) != 0; }
  bool SetEquals(const BitSet& other) const { return uint8_bits_ == other.uint8_bits_; }
  bool Contains(std::string_view sv_value) const {
    return BitSetAllocator<Tag>::BitsContainString(uint8_bits_, sv_value);
  }

  BitSet Except(const BitSet& other) const { return FromBits(uint8_bits_ & ~other.uint8_bits_); }
  BitSet Intersect(const BitSet& other) const { return FromBits(uint8_bits_ & other.uint8_bits_); }
  BitSet SymmetricExcept(const BitSet& other) const { return FromBits(uint8_bits_ ^ other.uint8_bits_); }
  BitSet Union(const BitSet& other) const { return FromBits(uint8_bits_ | other.uint8_bits_); }

  bool operator==(const BitSet&) const = default;

  /// 位图直读(描述表加载/FieldSaver 需要原始位图)
  /// Raw bit access (descriptor-table loading / FieldSaver need the raw map).
  std::uint64_t Bits() const { return uint8_bits_; }

  /// 按位构造(meta 描述表加载:所有 BitSet<Tag> 布局相同,经注册表按位写入)
  /// Construct from raw bits (meta descriptor loading: all BitSet<Tag> share
  /// one layout; bits are written through the registry).
  static BitSet FromBits(std::uint64_t uint8_bits) {
    BitSet set_ret;
    set_ret.uint8_bits_ = uint8_bits;
    return set_ret;
  }

 private:
  std::uint64_t uint8_bits_{};
};

}  // namespace ora::core
