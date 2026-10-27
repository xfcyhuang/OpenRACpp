#include "net/nat.hpp"

namespace ora::net {

Nat& Nat::Instance() {
  static Nat instance_nat;
  return instance_nat;
}

NatStatus Nat::Status() const {
  if (!b_searching_)
    return NatStatus::Disabled;
  return b_device_found_ ? NatStatus::Enabled : NatStatus::NotSupported;
}

void Nat::Initialize(bool b_discover_nat_devices) {
  if (b_initialized_)
    return;

  if (b_discover_nat_devices)
    b_searching_ = true;

  b_initialized_ = true;
}

bool Nat::TryForwardPort(int int4_listen, int int4_external) {
  static_cast<void>(int4_listen);
  static_cast<void>(int4_external);
  return false;
}

bool Nat::TryRemovePortForward() {
  return false;
}

void Nat::SetDeviceFoundForTest(bool b_found) {
  b_device_found_ = b_found;
}

void Nat::ResetForTest() {
  b_initialized_ = false;
  b_searching_ = false;
  b_device_found_ = false;
}

}
