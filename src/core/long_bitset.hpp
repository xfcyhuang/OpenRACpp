// UPSTREAM: OpenRA.Game/Primitives/LongBitSet.cs @b6fc03f(逐语义重写)
//          Verbatim-semantics rewrite.
//
// 机制对照 / Mechanism mapping:
//  - LongBitSetAllocator<T> 的 Cache<string,long> + nextBits 位分配器:
//    键序非契约(仅存在性/位查询消费)→ std::map 插入序承载;溢出
//    OverflowException "Trying to allocate bit index outside of index 64."
//    同文本;Reset() 清表回卷(World 构造的 LongBitSet<PlayerBitMask>.Reset()
//    锚点 —— C++ 进程级单例即分配域,World 复用同表;上游每世界复位由
//    位身份仅在世界内消费保证等价 —— COVERAGE 登记)
//    The LongBitSetAllocator<T> Cache<string,long> + nextBits allocator:
//    the key order is non-contractual (only membership/bit queries are
//    consumed) → a std::map; the overflow keeps the
//    "Trying to allocate bit index outside of index 64." text; Reset()
//    clears and rewinds (the World ctor's LongBitSet<PlayerBitMask>.Reset()
//    anchor — the C++ process singleton is the allocation domain, world
//    reuse of the same table is equivalent because bit identities are only
//    consumed within one world — registered in COVERAGE).
//  - LongBitSet<T> 的 readonly struct 位集合族:逐运算照抄(Union/Intersect/
//    Except/SymmetricExcept/Overlaps/IsSubsetOf/IsSupersetOf/SetEquals/
//    IsProperSubsetOf/IsProperSupersetOf/IsEmpty/Contains/ToString)
//    The readonly-struct bit-set family of LongBitSet<T>: every operation
//    copied verbatim.
#pragma once
import std;

namespace ora {

/// LongBitSetAllocator<T>(T = 标签类;LongBitSet.cs L19-90)
/// LongBitSetAllocator<T> (T = the tag class; LongBitSet.cs L19-90).
template <class Tag>
class LongBitSetAllocator {
 public:
  /// Allocate(L25-37):nextBits 移满抛 | Allocate (L25-37): nextBits full
  /// throws.
  static std::int64_t Allocate(const std::string& str_value) {
    const std::int64_t bits = next_bits_;
    next_bits_ <<= 1;

    if (next_bits_ == 0)
      throw std::overflow_error(
          "Trying to allocate bit index outside of index 64.");

    map_bits_.emplace(str_value, bits);
    return bits;
  }

  /// GetBits(L39-47) | GetBits (L39-47).
  static std::int64_t GetBits(std::span<const std::string> values) {
    std::int64_t bits = 0;
    for (const std::string& value : values) {
      auto it = map_bits_.find(value);
      if (it == map_bits_.end())
        it = map_bits_.emplace(value, Allocate(value)).first;
      bits |= it->second;
    }
    return bits;
  }

  /// GetBitsNoAlloc(L49-60) | GetBitsNoAlloc (L49-60).
  static std::int64_t GetBitsNoAlloc(std::span<const std::string> values) {
    // Map strings to existing bits; do not allocate missing values new bits
    // (上游注释)| (upstream comment).
    std::int64_t bits = 0;
    for (const std::string& value : values) {
      auto it = map_bits_.find(value);
      if (it != map_bits_.end())
        bits |= it->second;
    }
    return bits;
  }

  /// GetStrings(L62-71):枚举序 = Cache 实现序(插入序)| GetStrings
  /// (L62-71): the enumeration order is the cache's (insertion order).
  static std::vector<std::string> GetStrings(std::int64_t bits) {
    std::vector<std::string> values;
    for (const auto& [key, value_bits] : map_bits_)
      if ((value_bits & bits) != 0)
        values.push_back(key);
    return values;
  }

  /// BitsContainString(L73-80) | BitsContainString (L73-80).
  static bool BitsContainString(std::int64_t bits,
                                const std::string& str_value) {
    auto it = map_bits_.find(str_value);
    if (it == map_bits_.end())
      return false;
    return (bits & it->second) != 0;
  }

  /// Reset(L82-89) | Reset (L82-89).
  static void Reset() {
    map_bits_.clear();
    next_bits_ = 1;
  }

 private:
  static inline std::map<std::string, std::int64_t> map_bits_;
  static inline std::int64_t next_bits_ = 1;
};

/// LongBitSet<T>(L93-190;readonly struct → 值类型)
/// LongBitSet<T> (L93-190; the readonly struct → a value type).
template <class Tag>
class LongBitSet {
 public:
  /// LongBitSet(params string[])(L97-98) | the params ctor (L97-98).
  explicit LongBitSet(std::span<const std::string> values)
      : bits_{LongBitSetAllocator<Tag>::GetBits(values)} {}
  LongBitSet() = default;

  /// FromStringsNoAlloc(L102-105) | FromStringsNoAlloc (L102-105).
  static LongBitSet FromStringsNoAlloc(std::span<const std::string> values) {
    return LongBitSet{LongBitSetAllocator<Tag>::GetBitsNoAlloc(values)};
  }

  /// Reset(L107-110) | Reset (L107-110).
  static void Reset() { LongBitSetAllocator<Tag>::Reset(); }

  /// ToString(L112-115):JoinWith(",") | ToString (L112-115): JoinWith(",").
  std::string ToString() const {
    const std::vector<std::string> values =
        LongBitSetAllocator<Tag>::GetStrings(bits_);
    std::string joined;
    for (std::size_t i = 0; i < values.size(); i++) {
      if (i != 0)
        joined += ',';
      joined += values[i];
    }
    return joined;
  }

  friend bool operator==(LongBitSet a, LongBitSet b) {
    return a.bits_ == b.bits_;
  }
  friend bool operator!=(LongBitSet a, LongBitSet b) { return !(a == b); }

  bool IsEmpty() const { return bits_ == 0; }                              // L124
  bool IsProperSubsetOf(LongBitSet other) const {                          // L126-129
    return IsSubsetOf(other) && !SetEquals(other);
  }
  bool IsProperSupersetOf(LongBitSet other) const {                        // L131-134
    return IsSupersetOf(other) && !SetEquals(other);
  }
  bool IsSubsetOf(LongBitSet other) const {                                // L136-139
    return (bits_ | other.bits_) == other.bits_;
  }
  bool IsSupersetOf(LongBitSet other) const {                              // L141-144
    return (bits_ | other.bits_) == bits_;
  }
  bool Overlaps(LongBitSet other) const {                                  // L146-149
    return (bits_ & other.bits_) != 0;
  }
  bool SetEquals(LongBitSet other) const { return bits_ == other.bits_; }  // L151-154

  bool Contains(const std::string& str_value) const {                      // L156-159
    return LongBitSetAllocator<Tag>::BitsContainString(bits_, str_value);
  }

  LongBitSet Except(LongBitSet other) const {                              // L171-174
    return LongBitSet{bits_ & ~other.bits_};
  }
  LongBitSet Intersect(LongBitSet other) const {                           // L176-179
    return LongBitSet{bits_ & other.bits_};
  }
  LongBitSet SymmetricExcept(LongBitSet other) const {                     // L181-184
    return LongBitSet{bits_ ^ other.bits_};
  }
  LongBitSet Union(LongBitSet other) const {                               // L186-189
    return LongBitSet{bits_ | other.bits_};
  }

  /// 位域直读(HasRelationship 等高速面的等价物;值语义保持)
  /// The raw-bits read (the equivalent of high-speed faces; the value
  /// semantics stay).
  std::int64_t Bits() const { return bits_; }

 private:
  explicit LongBitSet(std::int64_t bits) : bits_{bits} {}  // L100

  std::int64_t bits_ = 0;
};

}  // namespace ora
