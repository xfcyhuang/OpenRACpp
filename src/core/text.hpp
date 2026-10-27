#pragma once
import std;

namespace ora {

constexpr bool IsNetWhiteSpace(char32_t cp) {
  return (cp >= 0x0009 && cp <= 0x000D) || cp == 0x0020 || cp == 0x0085 || cp == 0x00A0 ||
         cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200A) || cp == 0x2028 || cp == 0x2029 ||
         cp == 0x202F || cp == 0x205F || cp == 0x3000;
}

constexpr char32_t DecodeUtf8(std::string_view text, std::size_t& int8_i) {
  const unsigned char b0{static_cast<unsigned char>(text[int8_i])};
  const std::size_t int8_start{int8_i};
  if (b0 < 0x80) {
    int8_i += 1;
    return b0;
  }
  std::size_t int8_len{};
  char32_t cp{};
  if ((b0 & 0xE0) == 0xC0) {
    int8_len = 2;
    cp = b0 & 0x1F;
  } else if ((b0 & 0xF0) == 0xE0) {
    int8_len = 3;
    cp = b0 & 0x0F;
  } else if ((b0 & 0xF8) == 0xF0) {
    int8_len = 4;
    cp = b0 & 0x07;
  } else {
    int8_i += 1;
    return 0xFFFD;
  }
  if (int8_start + int8_len > text.size()) {
    int8_i += 1;
    return 0xFFFD;
  }
  for (std::size_t k{1}; k < int8_len; k++) {
    const unsigned char bk{static_cast<unsigned char>(text[int8_start + k])};
    if ((bk & 0xC0) != 0x80) {
      int8_i += 1;
      return 0xFFFD;
    }
    cp = (cp << 6) | (bk & 0x3F);
  }

  if (cp > 0x10FFFF || (int8_len == 2 && cp < 0x80) || (int8_len == 3 && cp < 0x800) ||
      (int8_len == 4 && cp < 0x10000)) {
    int8_i += 1;
    return 0xFFFD;
  }
  int8_i += int8_len;
  return cp;
}

constexpr std::string_view TrimNetWhiteSpace(std::string_view text) {
  std::size_t int8_begin{0};
  while (int8_begin < text.size()) {
    std::size_t int8_next{int8_begin};
    if (!IsNetWhiteSpace(DecodeUtf8(text, int8_next)))
      break;
    int8_begin = int8_next;
  }
  std::size_t int8_end{text.size()};
  while (int8_end > int8_begin) {

    std::size_t int8_p{int8_end - 1};
    while (int8_p > int8_begin && (static_cast<unsigned char>(text[int8_p]) & 0xC0) == 0x80)
      int8_p--;
    const std::size_t int8_cpStart{int8_p};
    if (!IsNetWhiteSpace(DecodeUtf8(text, int8_p)))
      break;
    int8_end = int8_cpStart;
  }
  return text.substr(int8_begin, int8_end - int8_begin);
}

constexpr std::string_view TrimEndNetWhiteSpace(std::string_view text) {
  std::size_t int8_end{text.size()};
  while (int8_end > 0) {
    std::size_t int8_p{int8_end - 1};
    while (int8_p > 0 && (static_cast<unsigned char>(text[int8_p]) & 0xC0) == 0x80)
      int8_p--;
    const std::size_t int8_cpStart{int8_p};
    if (!IsNetWhiteSpace(DecodeUtf8(text, int8_p)))
      break;
    int8_end = int8_cpStart;
  }
  return text.substr(0, int8_end);
}

constexpr std::string_view TrimStartNetWhiteSpace(std::string_view text) {
  std::size_t int8_begin{0};
  while (int8_begin < text.size()) {
    std::size_t int8_next{int8_begin};
    if (!IsNetWhiteSpace(DecodeUtf8(text, int8_next)))
      break;
    int8_begin = int8_next;
  }
  return text.substr(int8_begin);
}

}
