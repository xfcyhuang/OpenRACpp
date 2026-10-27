#include "fluent/fluent_provider.hpp"

#include "yaml/mini_yaml.hpp"

namespace ora::fluent {
namespace {

int Base64Digit(char ch) {
  if (ch >= 'A' && ch <= 'Z')
    return ch - 'A';
  if (ch >= 'a' && ch <= 'z')
    return ch - 'a' + 26;
  if (ch >= '0' && ch <= '9')
    return ch - '0' + 52;
  if (ch == '+')
    return 62;
  if (ch == '/')
    return 63;
  return -1;
}

std::optional<std::string> DecodeBase64(std::string_view sv) {
  std::string str_out;
  str_out.reserve(sv.size() / 4 * 3);
  int int_buf = 0;
  int int_bits = 0;
  for (const char ch : sv) {
    if (ch == '=')
      break;
    const int int_digit = Base64Digit(ch);
    if (int_digit < 0)
      return std::nullopt;
    int_buf = (int_buf << 6) | int_digit;
    int_bits += 6;
    if (int_bits >= 8) {
      int_bits -= 8;
      str_out.push_back(static_cast<char>((int_buf >> int_bits) & 0xFF));
    }
  }
  return str_out;
}

}

void FluentProvider::Initialize(std::string_view str_culture,
                                std::span<const std::string> vec_paths,
                                FileOpenFn fn_open,
                                const yaml::MiniYaml* ptr_map_definitions,
                                ErrorSink fn_on_error) {
  const std::lock_guard<std::mutex> guard_lock(mtx_sync_);
  ptr_mod_bundle_ = std::make_unique<FluentBundle>(
      std::string{str_culture}, vec_paths, std::move(fn_open),
      std::string_view{}, std::move(fn_on_error));

  ptr_map_bundle_ = nullptr;
  if (ptr_map_definitions != nullptr) {
    std::vector<std::string> vec_map_files;
    if (ptr_map_definitions->Value != nullptr) {

      for (const auto part :
           std::views::split(std::string_view{*ptr_map_definitions->Value}, ',')) {
        std::string_view str_part{part.begin(), part.end()};
        const std::size_t uint_first = str_part.find_first_not_of(" \t");
        const std::size_t uint_last = str_part.find_last_not_of(" \t");
        if (uint_first == std::string_view::npos)
          continue;
        str_part = str_part.substr(uint_first, uint_last - uint_first + 1);
        vec_map_files.emplace_back(str_part);
      }
    }

    std::string str_map_text;
    if (!ptr_map_definitions->Nodes.empty()) {
      for (const yaml::MiniYamlNode& node : ptr_map_definitions->Nodes) {
        if (node.Key != nullptr && *node.Key == "base64" &&
            node.Value.Value != nullptr) {
          if (const std::optional<std::string> opt_decoded =
                  DecodeBase64(*node.Value.Value))
            str_map_text.append(*opt_decoded);
        }
      }
    }

    ptr_map_bundle_ = std::make_unique<FluentBundle>(
        std::string{str_culture}, vec_map_files, nullptr, str_map_text,
        ErrorSink{});
  }
}

std::string FluentProvider::GetMessage(
    std::string_view str_key,
    std::span<const std::pair<std::string, FluentValue>> vec_args) {
  const std::lock_guard<std::mutex> guard_lock(mtx_sync_);
  if (ptr_mod_bundle_ != nullptr) {
    std::string str_message;
    if (ptr_mod_bundle_->TryGetMessage(str_key, vec_args, str_message))
      return str_message;
  }
  if (ptr_map_bundle_ != nullptr) {
    std::string str_message;
    if (ptr_map_bundle_->TryGetMessage(str_key, vec_args, str_message))
      return str_message;
  }
  return std::string{str_key};
}

bool FluentProvider::TryGetMessage(
    std::string_view str_key,
    std::span<const std::pair<std::string, FluentValue>> vec_args,
    std::string& out_message) {
  const std::lock_guard<std::mutex> guard_lock(mtx_sync_);
  if (ptr_mod_bundle_ != nullptr &&
      ptr_mod_bundle_->TryGetMessage(str_key, vec_args, out_message))
    return true;
  if (ptr_map_bundle_ != nullptr &&
      ptr_map_bundle_->TryGetMessage(str_key, vec_args, out_message))
    return true;
  out_message.clear();
  return false;
}

bool FluentProvider::TryGetModMessage(
    std::string_view str_key,
    std::span<const std::pair<std::string, FluentValue>> vec_args,
    std::string& out_message) {
  const std::lock_guard<std::mutex> guard_lock(mtx_sync_);
  if (ptr_mod_bundle_ != nullptr)
    return ptr_mod_bundle_->TryGetMessage(str_key, vec_args, out_message);
  out_message.clear();
  return false;
}

void FluentProvider::ResetForTest() {
  const std::lock_guard<std::mutex> guard_lock(mtx_sync_);
  ptr_mod_bundle_ = nullptr;
  ptr_map_bundle_ = nullptr;
}

}
