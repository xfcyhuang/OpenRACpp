#pragma once
import std;

namespace ora::core {

template <class Tag>
class BitSetAllocator {
 public:

  static std::uint64_t GetBits(std::span<const std::string> vec_values) {
    if (Bridge().fn_get_bits)
      return Bridge().fn_get_bits(vec_values);
    std::uint64_t uint8_bits{0};
    for (const std::string& str_value : vec_values)
      uint8_bits |= BitFor(str_value);
    return uint8_bits;
  }

  static std::uint64_t GetBitsNoAlloc(std::span<const std::string> vec_values) {
    if (Bridge().fn_get_bits_noalloc) {
      if (const auto bits = Bridge().fn_get_bits_noalloc(vec_values))
        return *bits;
      return 0;
    }
    std::uint64_t uint8_bits{0};
    auto& map_index = Index();
    for (const std::string& str_value : vec_values) {
      if (const auto it_find = map_index.find(str_value); it_find != map_index.end())
        uint8_bits |= std::uint64_t{1} << it_find->second;
    }
    return uint8_bits;
  }

  static std::vector<std::string> GetStrings(std::uint64_t uint8_bits) {
    if (Bridge().fn_get_strings)
      return Bridge().fn_get_strings(uint8_bits);
    std::vector<std::string> vec_values;
    const std::vector<std::string>& vec_order = Order();
    for (std::size_t int4_i{}; int4_i < vec_order.size(); int4_i++)
      if ((uint8_bits & (std::uint64_t{1} << int4_i)) != 0)
        vec_values.push_back(vec_order[int4_i]);
    return vec_values;
  }

  static bool BitsContainString(std::uint64_t uint8_bits, std::string_view sv_value) {
    if (Bridge().fn_contains)
      return Bridge().fn_contains(uint8_bits, sv_value);
    auto& map_index = Index();
    if (const auto it_find = map_index.find(std::string{sv_value}); it_find != map_index.end())
      return (uint8_bits & (std::uint64_t{1} << it_find->second)) != 0;
    return false;
  }

  static void InstallBridge(
      std::function<std::uint64_t(std::span<const std::string>)> fn_get_bits,
      std::function<std::vector<std::string>(std::uint64_t)> fn_get_strings,
      std::function<bool(std::uint64_t, std::string_view)> fn_contains,
      std::function<std::optional<std::uint64_t>(std::span<const std::string>)>
          fn_get_bits_noalloc = {}) {
    Bridge() = BridgeData{std::move(fn_get_bits), std::move(fn_get_strings),
                          std::move(fn_contains),
                          std::move(fn_get_bits_noalloc)};
  }

 private:
  struct BridgeData {
    std::function<std::uint64_t(std::span<const std::string>)> fn_get_bits;
    std::function<std::vector<std::string>(std::uint64_t)> fn_get_strings;
    std::function<bool(std::uint64_t, std::string_view)> fn_contains;
    std::function<std::optional<std::uint64_t>(std::span<const std::string>)>
        fn_get_bits_noalloc;
  };
  static BridgeData& Bridge() {
    static BridgeData data_bridge;
    return data_bridge;
  }

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

    if (int4_next >= 64)
      std::abort();
    vec_order.push_back(str_value);
    map_index.emplace(str_value, int4_next);
    return std::uint64_t{1} << int4_next;
  }
};

template <class Tag>
class BitSet {
 public:
  BitSet() = default;
  explicit BitSet(std::span<const std::string> vec_values)
      : uint8_bits_{BitSetAllocator<Tag>::GetBits(vec_values)} {}

  static BitSet FromStringsNoAlloc(std::span<const std::string> vec_values) {
    BitSet set_ret;
    set_ret.uint8_bits_ = BitSetAllocator<Tag>::GetBitsNoAlloc(vec_values);
    return set_ret;
  }

  static BitSet FromRawBits(std::uint64_t uint8_bits) {
    BitSet set_ret;
    set_ret.uint8_bits_ = uint8_bits;
    return set_ret;
  }

  std::uint64_t RawBits() const { return uint8_bits_; }

  std::string ToString() const {
    std::string str_ret;
    for (const std::string& str_v : BitSetAllocator<Tag>::GetStrings(uint8_bits_)) {
      if (!str_ret.empty())
        str_ret += ',';
      str_ret += str_v;
    }
    return str_ret;
  }

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

  std::uint64_t Bits() const { return uint8_bits_; }

  static BitSet FromBits(std::uint64_t uint8_bits) {
    BitSet set_ret;
    set_ret.uint8_bits_ = uint8_bits;
    return set_ret;
  }

 private:
  std::uint64_t uint8_bits_{};
};

}
