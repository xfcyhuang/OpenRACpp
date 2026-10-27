#pragma once
import std;

#include "formats/span_reader.hpp"

namespace ora::fmt {

class XccLocalDatabase {
 public:

  explicit XccLocalDatabase(std::span<const std::byte> vec_source);

  static std::vector<std::byte> MakeData(std::span<const std::string> vec_entries);

  const std::vector<std::string>& Entries() const { return vec_entries_; }

 private:
  std::vector<std::string> vec_entries_;
};

class XccGlobalDatabase {
 public:
  explicit XccGlobalDatabase(std::span<const std::byte> vec_source);

  const std::vector<std::string>& Entries() const { return vec_entries_; }

 private:
  std::vector<std::string> vec_entries_;
};

}
