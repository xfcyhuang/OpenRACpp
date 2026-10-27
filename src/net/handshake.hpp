#pragma once
import std;

#include "net/session.hpp"

namespace ora::net {

struct HandshakeRequest {
  std::string Mod;
  std::string Version;
  std::string AuthToken;

  static std::optional<HandshakeRequest> Deserialize(
      std::string_view str_data, std::string_view str_name);

  std::string Serialize() const;
};

struct HandshakeResponse {
  std::string Mod;
  std::string Version;
  std::string Password;
  int OrdersProtocol = 7;

  std::string Fingerprint;
  std::string AuthSignature;

  SessionClient Client{};

  static std::optional<HandshakeResponse> Deserialize(
      std::string_view str_data, std::string_view str_name);

  std::string Serialize() const;
};

}
