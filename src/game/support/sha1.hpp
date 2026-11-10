#pragma once

import std;

namespace ora::game::support {

struct Sha1Digest {
  std::array<std::uint8_t, 20> vec1u_bytes{};
};

[[nodiscard]] Sha1Digest sha1_digest(std::span<const std::uint8_t> span1u_data);

[[nodiscard]] std::string sha1_hex(std::span<const std::uint8_t> span1u_data);

[[nodiscard]] std::string sha1_hex(std::string_view str_data);

}
