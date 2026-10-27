#pragma once
import std;

namespace ora {

template <class Tag>
class LongBitSetAllocator {
 public:

  static std::int64_t Allocate(const std::string& str_value) {
    const std::int64_t bits = next_bits_;
    next_bits_ <<= 1;

    if (next_bits_ == 0)
      throw std::overflow_error(
          "Trying to allocate bit index outside of index 64.");

    map_bits_.emplace(str_value, bits);
    return bits;
  }

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

  static std::int64_t GetBitsNoAlloc(std::span<const std::string> values) {

    std::int64_t bits = 0;
    for (const std::string& value : values) {
      auto it = map_bits_.find(value);
      if (it != map_bits_.end())
        bits |= it->second;
    }
    return bits;
  }

  static std::vector<std::string> GetStrings(std::int64_t bits) {
    std::vector<std::string> values;
    for (const auto& [key, value_bits] : map_bits_)
      if ((value_bits & bits) != 0)
        values.push_back(key);
    return values;
  }

  static bool BitsContainString(std::int64_t bits,
                                const std::string& str_value) {
    auto it = map_bits_.find(str_value);
    if (it == map_bits_.end())
      return false;
    return (bits & it->second) != 0;
  }

  static void Reset() {
    map_bits_.clear();
    next_bits_ = 1;
  }

 private:
  static inline std::map<std::string, std::int64_t> map_bits_;
  static inline std::int64_t next_bits_ = 1;
};

template <class Tag>
class LongBitSet {
 public:

  explicit LongBitSet(std::span<const std::string> values)
      : bits_{LongBitSetAllocator<Tag>::GetBits(values)} {}
  LongBitSet() = default;

  static LongBitSet FromStringsNoAlloc(std::span<const std::string> values) {
    return LongBitSet{LongBitSetAllocator<Tag>::GetBitsNoAlloc(values)};
  }

  static void Reset() { LongBitSetAllocator<Tag>::Reset(); }

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

  bool IsEmpty() const { return bits_ == 0; }
  bool IsProperSubsetOf(LongBitSet other) const {
    return IsSubsetOf(other) && !SetEquals(other);
  }
  bool IsProperSupersetOf(LongBitSet other) const {
    return IsSupersetOf(other) && !SetEquals(other);
  }
  bool IsSubsetOf(LongBitSet other) const {
    return (bits_ | other.bits_) == other.bits_;
  }
  bool IsSupersetOf(LongBitSet other) const {
    return (bits_ | other.bits_) == bits_;
  }
  bool Overlaps(LongBitSet other) const {
    return (bits_ & other.bits_) != 0;
  }
  bool SetEquals(LongBitSet other) const { return bits_ == other.bits_; }

  bool Contains(const std::string& str_value) const {
    return LongBitSetAllocator<Tag>::BitsContainString(bits_, str_value);
  }

  LongBitSet Except(LongBitSet other) const {
    return LongBitSet{bits_ & ~other.bits_};
  }
  LongBitSet Intersect(LongBitSet other) const {
    return LongBitSet{bits_ & other.bits_};
  }
  LongBitSet SymmetricExcept(LongBitSet other) const {
    return LongBitSet{bits_ ^ other.bits_};
  }
  LongBitSet Union(LongBitSet other) const {
    return LongBitSet{bits_ | other.bits_};
  }

  std::int64_t Bits() const { return bits_; }

 private:
  explicit LongBitSet(std::int64_t bits) : bits_{bits} {}

  std::int64_t bits_ = 0;
};

}
