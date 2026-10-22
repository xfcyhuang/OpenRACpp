// UPSTREAM: OpenRA.Game/Network/Nat.cs @e180e3225 L18-101(全文;
//          Mono.Nat 的 UPnP/NAT-PMP 发现链 → 极小承载:发现从不启动或
//          从无设备两态 + 端口映射恒 false —— Mono.Nat 依赖的 C++ 侧
//          等价域登记于 docs/COVERAGE.md)
//          The whole of Nat.cs L18-101 (the Mono.Nat UPnP/NAT-PMP discovery
//          chain becomes a minimal carrier: the never-started/never-found
//          pair of states + port mappings that always fail — the C++-side
//          equivalence domain of the Mono.Nat dependency is registered in
//          docs/COVERAGE.md).
#pragma once
import std;

namespace ora::net {

/// Nat.cs L18
enum class NatStatus { Enabled = 0, Disabled = 1, NotSupported = 2 };

/// Nat(静态类 → 进程级单例)| Nat (the static class becomes a process
/// singleton).
class Nat final {
 public:
  static Nat& Instance();

  Nat(const Nat&) = delete;
  Nat& operator=(const Nat&) = delete;

  /// Status(L22):搜索中(含找到 = Enabled / 未找到 = NotSupported);
  /// 未搜索 = Disabled
  /// Status (L22): while searching (found = Enabled / not found =
  /// NotSupported); not searching = Disabled.
  NatStatus Status() const;

  /// Initialize(L28-40):DiscoverNatDevices 时启动发现(Mono.Nat 缺席 =
  /// 搜索置位而无设备)| Initialize (L28-40): starts the discovery when
  /// DiscoverNatDevices (Mono.Nat's absence = the search flag set with no
  /// device).
  void Initialize(bool b_discover_nat_devices);

  /// TryForwardPort(L61-80):无设备 = false(Mono.Nat 缺席恒 false)
  /// TryForwardPort (L61-80): no device = false (constantly false in
  /// Mono.Nat's absence).
  bool TryForwardPort(int int4_listen, int int4_external);

  /// TryRemovePortForward(L82-99):同上恒 false | ditto, constantly false.
  bool TryRemovePortForward();

  void SetDeviceFoundForTest(bool b_found);
  void ResetForTest();

 private:
  Nat() = default;
  ~Nat() = default;

  bool b_initialized_ = false;
  bool b_searching_ = false;
  bool b_device_found_ = false;
};

}  // namespace ora::net
