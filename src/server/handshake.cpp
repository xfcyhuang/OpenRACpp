// UPSTREAM: OpenRA.Game/Network/Handshake.cs @e180e3225 L17-90(字段装载/
//          序列化全文;FieldLoader/FieldSaver → 按名手写表,与 Session 同形)
//          Handshake.cs L17-90 (the whole load/serialize text; FieldLoader/
//          FieldSaver → the by-name hand-written tables, the same shape as
//          Session's).
import std;

#include "server/handshake.hpp"

#include "yaml/mini_yaml.hpp"

namespace ora::server {

namespace {

yaml::MiniYamlNode MakeHandshakeLeaf(yaml::StringPool& pool,
                                     std::string_view str_key,
                                     std::string_view str_text) {
  return yaml::MiniYamlNode{pool.Intern(str_key),
                            yaml::MiniYaml{pool.Intern(str_text)}};
}

bool LoadHandshakeFields(const yaml::MiniYaml& yaml_v,
                         std::string& str_mod_out, std::string& str_version_out,
                         std::string& str_password_out,
                         std::string& str_fingerprint_out,
                         std::string& str_auth_signature_out,
                         int& int4_orders_protocol_out) {
  for (const auto& node : yaml_v.Nodes) {
    const std::string_view key{*node.Key};
    const std::string text{
        node.Value.Value != nullptr ? *node.Value.Value : std::string{}};
    if (key == "Mod")
      str_mod_out = text;
    else if (key == "Version")
      str_version_out = text;
    else if (key == "Password")
      str_password_out = text;
    else if (key == "Fingerprint")
      str_fingerprint_out = text;
    else if (key == "AuthSignature")
      str_auth_signature_out = text;
    else if (key == "OrdersProtocol") {
      const auto parsed = std::from_chars(text.data(), text.data() + text.size(),
                                          int4_orders_protocol_out);
      if (parsed.ec != std::errc{})
        return false;
    }
  }
  return true;
}

}  // namespace

std::optional<HandshakeRequest> HandshakeRequest::Deserialize(
    std::string_view str_data, std::string_view str_name) {
  const auto vec_nodes = yaml::MiniYaml::FromString(str_data, str_name);
  if (vec_nodes.empty())
    return std::nullopt;

  auto request = HandshakeRequest{};
  for (const auto& node : vec_nodes.front().Value.Nodes) {
    const std::string_view key{*node.Key};
    const std::string text{
        node.Value.Value != nullptr ? *node.Value.Value : std::string{}};
    if (key == "Mod")
      request.Mod = text;
    else if (key == "Version")
      request.Version = text;
    else if (key == "AuthToken")
      request.AuthToken = text;
  }
  return request;
}

std::string HandshakeRequest::Serialize() const {
  auto pool = yaml::StringPool{};
  auto vec_fields = std::vector<yaml::MiniYamlNode>{};
  vec_fields.push_back(MakeHandshakeLeaf(pool, "Mod", Mod));
  vec_fields.push_back(MakeHandshakeLeaf(pool, "Version", Version));
  vec_fields.push_back(MakeHandshakeLeaf(pool, "AuthToken", AuthToken));

  auto vec_root = std::vector<yaml::MiniYamlNode>{};
  vec_root.push_back(yaml::MiniYamlNode{
      pool.Intern("Handshake"), yaml::MiniYaml{nullptr, std::move(vec_fields)}});
  return yaml::WriteToString(vec_root);
}

std::optional<HandshakeResponse> HandshakeResponse::Deserialize(
    std::string_view str_data, std::string_view str_name) {
  const auto vec_nodes = yaml::MiniYaml::FromString(str_data, str_name);

  auto response = HandshakeResponse{};
  for (const auto& node : vec_nodes) {
    const std::string_view key{*node.Key};
    if (key == "Handshake") {
      auto unused_password = std::string{};
      auto unused_fingerprint = std::string{};
      auto unused_signature = std::string{};
      if (!LoadHandshakeFields(node.Value, response.Mod, response.Version,
                               unused_password, unused_fingerprint,
                               unused_signature, response.OrdersProtocol))
        return std::nullopt;

      response.Password = unused_password;
      response.Fingerprint = unused_fingerprint;
      response.AuthSignature = unused_signature;
    } else if (key == "Client") {
      if (!net::LoadSessionClient(node.Value, response.Client))
        return std::nullopt;
    }
  }

  return response;
}

std::string HandshakeResponse::Serialize() const {
  auto pool = yaml::StringPool{};

  auto vec_handshake_fields = std::vector<yaml::MiniYamlNode>{};
  vec_handshake_fields.push_back(MakeHandshakeLeaf(pool, "Mod", Mod));
  vec_handshake_fields.push_back(MakeHandshakeLeaf(pool, "Version", Version));
  vec_handshake_fields.push_back(MakeHandshakeLeaf(pool, "Password", Password));
  vec_handshake_fields.push_back(
      MakeHandshakeLeaf(pool, "Fingerprint", Fingerprint));
  vec_handshake_fields.push_back(
      MakeHandshakeLeaf(pool, "AuthSignature", AuthSignature));
  vec_handshake_fields.push_back(MakeHandshakeLeaf(
      pool, "OrdersProtocol", std::to_string(OrdersProtocol)));

  auto vec_root = std::vector<yaml::MiniYamlNode>{};
  vec_root.push_back(yaml::MiniYamlNode{
      pool.Intern("Handshake"),
      yaml::MiniYaml{nullptr, std::move(vec_handshake_fields)}});
  vec_root.push_back(net::SerializeSessionClientFields(pool, Client));
  return yaml::WriteToString(vec_root);
}

}  // namespace ora::server
