#pragma once
import std;

namespace ora::net {

enum class NatStatus { Enabled = 0, Disabled = 1, NotSupported = 2 };

class Nat final {
 public:
  static Nat& Instance();

  Nat(const Nat&) = delete;
  Nat& operator=(const Nat&) = delete;

  NatStatus Status() const;

  void Initialize(bool b_discover_nat_devices);

  bool TryForwardPort(int int4_listen, int int4_external);

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

}
