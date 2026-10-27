#include "yaml/mini_yaml.hpp"
#include <cstdio>

namespace {

namespace fs = std::filesystem;
using ora::yaml::MiniYaml;
using ora::yaml::MiniYamlNode;
using ora::yaml::StringPool;

struct GoldenEntry {
  std::string rel;
  std::string discardOut;
  std::string keepOut;
};

std::vector<std::string> SplitGoldenLines(const std::string& bytes) {
  std::vector<std::string> lines;
  std::size_t start = 0;
  while (start <= bytes.size()) {
    const std::size_t nl = bytes.find('\n', start);
    if (nl == std::string::npos) {
      if (start < bytes.size())
        lines.push_back(bytes.substr(start));
      break;
    }
    std::size_t end = nl;
    if (end > start && bytes[end - 1] == '\r')
      end--;
    lines.push_back(bytes.substr(start, end - start));
    start = nl + 1;
  }
  return lines;
}

std::vector<GoldenEntry> ParseGolden(const std::string& bytes) {
  std::vector<GoldenEntry> entries;
  GoldenEntry* cur = nullptr;
  std::string* block = nullptr;
  for (const std::string& line : SplitGoldenLines(bytes)) {
    if (!line.empty() && line[0] == '\x01') {

      if (line.starts_with("\x01" "F ")) {
        entries.emplace_back();
        cur = &entries.back();
        cur->rel = line.substr(3);
        block = nullptr;
      } else if (line == "\x01" "M discard") {
        block = &cur->discardOut;
      } else if (line == "\x01" "M keep") {
        block = &cur->keepOut;
      } else if (line.starts_with("\x01" "E ")) {
        if (block == nullptr)
          throw std::runtime_error{"golden 帧:错误块前无模式标记"};
        *block += line;
        *block += '\n';
      } else if (line == "\x01" "X") {
        cur = nullptr;
        block = nullptr;
      } else {
        throw std::runtime_error{"golden 帧:未知标记 " + line};
      }
    } else if (block != nullptr) {
      *block += line;
      *block += '\n';
    } else if (!line.empty()) {
      throw std::runtime_error{"golden 帧:内容出现在块外: " + line};
    }
  }
  return entries;
}

std::string ReadFileBytes(const fs::path& path) {
  std::ifstream f{path, std::ios::binary};
  if (!f)
    throw std::runtime_error{"无法读取: " + path.string()};
  return std::string{std::istreambuf_iterator<char>{f}, std::istreambuf_iterator<char>{}};
}

int failures = 0;

void CompareMode(const fs::path& repoRoot, const GoldenEntry& entry, bool keep,
                 const std::string& goldenOut, StringPool& pool) {
  const std::string bytes = ReadFileBytes(repoRoot / fs::path{entry.rel});
  std::string actual;
  try {
    auto nodes = MiniYaml::FromStream(bytes, entry.rel, keep, pool);
    actual = WriteToString(nodes);
  } catch (const ora::yaml::YamlException& e) {
    actual = std::format("\x01" "E YamlException: {}", e.what());
    actual += '\n';
  }
  if (actual != goldenOut) {
    failures++;
    std::println(std::cerr, "FAIL [{}] {} (mode={})", failures, entry.rel, keep ? "keep" : "discard");

    const auto actualLines = SplitGoldenLines(actual);
    const auto goldenLines = SplitGoldenLines(goldenOut);
    for (std::size_t i = 0; i < std::max(actualLines.size(), goldenLines.size()); i++) {
      const std::string& a = i < actualLines.size() ? actualLines[i] : "<EOF>";
      const std::string& g = i < goldenLines.size() ? goldenLines[i] : "<EOF>";
      if (a != g) {
        std::println(std::cerr, "  首差异 行{}:\n    期望: {}\n    实际: {}", i + 1, g, a);
        break;
      }
    }
  }
}

}

int main(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc != 3) {
    std::println(std::cerr, "用法: {} <golden_yaml.txt> <OpenRA 仓库根>", argv[0]);
    return 2;
  }
  const fs::path goldenPath{argv[1]};
  const fs::path repoRoot{argv[2]};

  const std::vector<GoldenEntry> entries = ParseGolden(ReadFileBytes(goldenPath));
  if (entries.empty()) {
    std::println(std::cerr, "黄金文件为空或解析失败: {}", goldenPath.string());
    return 2;
  }

  std::vector<std::string> rels;
  for (const auto& it : fs::recursive_directory_iterator(repoRoot / "mods")) {
    if (it.is_regular_file() && it.path().extension() == ".yaml") {
      auto rel = fs::relative(it.path(), repoRoot).generic_string();
      if (rel == it.path().generic_string())
        rel = it.path().generic_string();
      rels.push_back(rel);
    }
  }
  std::sort(rels.begin(), rels.end());
  rels.erase(std::unique(rels.begin(), rels.end()), rels.end());

  if (rels.size() != entries.size()) {
    std::println(std::cerr, "FAIL 文件数不一致:C# 黄金 {} 个,C++ 枚举 {} 个", entries.size(),
                 rels.size());
    return 1;
  }
  for (std::size_t i = 0; i < rels.size(); i++) {
    if (rels[i] != entries[i].rel) {
      std::println(std::cerr, "FAIL 枚举序不一致 @{}: 黄金={} 实际={}", i, entries[i].rel, rels[i]);
      return 1;
    }
  }

  StringPool pool;
  for (const GoldenEntry& entry : entries) {
    CompareMode(repoRoot, entry, false, entry.discardOut, pool);
    CompareMode(repoRoot, entry, true, entry.keepOut, pool);
  }

  if (failures != 0) {
    std::println(std::cerr, "结果: FAIL(共 {} 处不一致,样本见上)", failures);
    return 1;
  }
  std::println("golden_yaml PASS:{} 文件 × 2 模式,输出逐字节一致", entries.size());
  return 0;
}
