#include <miniz.h>

#include "fs/file_system.hpp"
#include "fs/folder.hpp"
#include "yaml/mini_yaml.hpp"
#include <cstdio>

namespace {

using ora::fs::FileSystem;
using ora::fs::Folder;
using ora::fs::IReadOnlyPackage;
using ora::yaml::MiniYaml;
using ora::yaml::MiniYamlNode;
using ora::yaml::StringPool;
using ora::yaml::WriteToString;

int checks = 0;
int failures = 0;

void CheckTrue(const std::string& name, bool cond, const std::string& what) {
  checks++;
  if (!cond) {
    failures++;
    std::println(std::cerr, "FAIL [{}] {}", name, what);
  }
}

void CheckEq(const std::string& name, const std::string& actual, const std::string& expected) {
  checks++;
  if (actual != expected) {
    failures++;
    std::println(std::cerr, "FAIL [{}] 输出不一致\n  期望: [{}]\n  实际: [{}]", name, expected,
                 actual);
  }
}

std::string Str(std::span<const char> v) { return {v.begin(), v.end()}; }

std::filesystem::path MakeTempDir(const std::string& tag) {
  const auto dir = std::filesystem::temp_directory_path() /
                   ("oracpp_fs_test_" + tag + "_" +
                    std::to_string(static_cast<long long>(
                        std::chrono::steady_clock::now().time_since_epoch().count())));
  std::filesystem::create_directories(dir);
  return dir;
}

void WriteText(const std::filesystem::path& p, std::string_view text) {
  std::filesystem::create_directories(p.parent_path());
  std::ofstream{p, std::ios::binary | std::ios::trunc} << text;
}

void TestFolder() {
  const auto dir = MakeTempDir("folder");
  {
    Folder folder{dir.generic_string()};
    folder.Update("b.yaml", std::vector<char>{'b'});
    folder.Update("a.yaml", std::vector<char>{'a'});
    folder.Update("sub/c.txt", std::vector<char>{'c'});

    const auto contents = folder.Contents();
    std::string joined;
    for (const std::string& c : contents)
      joined += c + ";";
    CheckEq("Folder.Contents 排序", joined, "a.yaml;b.yaml;sub;");

    CheckTrue("Folder.GetStream", folder.GetStream("a.yaml").has_value() &&
                                      Str(*folder.GetStream("a.yaml")) == "a",
              "a.yaml 内容");
    CheckTrue("Folder.Contains(子目录内)", folder.Contains("sub/c.txt"), "sub/c.txt 应存在");
    CheckTrue("Folder.Contains 越界拒绝", !folder.Contains("../outside.txt"),
              "路径穿越应拒绝");

    folder.Delete("sub/c.txt");
    CheckTrue("Folder.Delete", !folder.Contains("sub/c.txt"), "删除后不应存在");
  }
  std::filesystem::remove_all(dir);
}

void TestMountOrder() {
  const auto base = MakeTempDir("base");
  const auto over = MakeTempDir("over");
  WriteText(base / "a.yaml", "BASE");
  WriteText(base / "only.yaml", "BASEONLY");
  WriteText(over / "a.yaml", "OVER");

  FileSystem fs;
  fs.Mount(base.generic_string());
  fs.Mount(over.generic_string());

  CheckEq("挂载顺序=覆盖优先级", Str(fs.Open("a.yaml")), "OVER");
  CheckEq("基础包独有文件", Str(fs.Open("only.yaml")), "BASEONLY");
  CheckTrue("Exists", fs.Exists("a.yaml") && fs.Exists("only.yaml") &&
                          !fs.Exists("missing.yaml"),
            "Exists 语义");

  CheckEq("挂载数", std::to_string(fs.MountedPackageNames().size()), "2");
  CheckTrue("Unmount(unknown)=false", !fs.Unmount(nullptr), "未挂载指针应返回 false");

  IReadOnlyPackage* overPkg = nullptr;
  std::string sub;
  CheckTrue("TryGetPackageContaining", fs.TryGetPackageContaining("a.yaml", overPkg, sub),
            "应找到包含包");
  if (overPkg != nullptr) {
    fs.Unmount(overPkg);
    CheckEq("卸载后回落", Str(fs.Open("a.yaml")), "BASE");
  }

  std::filesystem::remove_all(base);
  std::filesystem::remove_all(over);
}

void TestExplicitMount() {
  const auto base = MakeTempDir("ebase");
  const auto over = MakeTempDir("eover");
  WriteText(base / "a.yaml", "BASE");
  WriteText(over / "a.yaml", "OVER");

  FileSystem fs;
  fs.Mount(base.generic_string());
  fs.Mount(std::make_unique<Folder>(over.generic_string()), "ovr");

  std::vector<char> bytes;
  CheckTrue("显式打开", fs.TryOpen("ovr|a.yaml", bytes) && Str(bytes) == "OVER",
            "ovr|a.yaml 应取覆盖包");
  CheckEq("普通打开仍走索引", Str(fs.Open("a.yaml")), "OVER");

  IReadOnlyPackage* pkg = nullptr;
  std::string sub;
  CheckTrue("TryGetPackageContaining('|')",
            fs.TryGetPackageContaining("ovr|a.yaml", pkg, sub) && pkg != nullptr && sub == "a.yaml",
            "显式拆分");

  std::filesystem::remove_all(base);
  std::filesystem::remove_all(over);
}

std::vector<char> MakeZip(const std::vector<std::pair<std::string, std::string>>& entries,
                          bool addDirPlaceholder = false) {
  mz_zip_archive zip{};
  if (!mz_zip_writer_init_heap(&zip, 0, 0))
    throw std::runtime_error{"zip writer init failed"};
  if (addDirPlaceholder)
    mz_zip_writer_add_mem(&zip, "sub/", nullptr, 0, MZ_DEFAULT_COMPRESSION);
  for (const auto& [name, data] : entries)
    mz_zip_writer_add_mem(&zip, name.c_str(), data.data(), data.size(), MZ_DEFAULT_COMPRESSION);
  void* buf = nullptr;
  std::size_t size = 0;
  mz_zip_writer_finalize_heap_archive(&zip, &buf, &size);
  mz_zip_writer_end(&zip);
  std::vector<char> out{static_cast<const char*>(buf), static_cast<const char*>(buf) + size};
  mz_free(buf);
  return out;
}

void TestZip() {

  const std::vector<char> inner = MakeZip({{"inner.txt", "INNER"}});
  const std::vector<char> outer = MakeZip(
      {{"rules.yaml", "Z"}, {"sub/inner.txt", "SUBTXT"}, {"nested.zip", {inner.begin(), inner.end()}}},
      true);

  const auto root = MakeTempDir("ziproot");
  const auto zipPath = root / "pack.zip";
  {
    std::ofstream f{zipPath, std::ios::binary | std::ios::trunc};
    f.write(outer.data(), static_cast<std::streamsize>(outer.size()));
  }

  FileSystem fs;
  fs.Mount(root.generic_string());
  fs.Mount("pack.zip", "pack");

  CheckEq("zip:文件条目", Str(fs.Open("rules.yaml")), "Z");
  CheckEq("zip:子目录文件", Str(fs.Open("sub/inner.txt")), "SUBTXT");
  CheckTrue("zip:Exists", fs.Exists("rules.yaml") && !fs.Exists("nope.txt"), "Exists");

  auto subPkg = fs.OpenPackage("pack|sub");
  CheckTrue("ZipFolder 打开", subPkg != nullptr, "sub 应可作为包打开");
  if (subPkg != nullptr) {
    const auto contents = subPkg->Contents();
    CheckEq("ZipFolder.Contents", contents.size() == 1 ? contents[0] : "<bad>", "inner.txt");
    CheckEq("ZipFolder.GetStream", subPkg->GetStream("inner.txt").has_value()
                                       ? Str(*subPkg->GetStream("inner.txt"))
                                       : "<null>",
            "SUBTXT");
  }

  auto nested = fs.OpenPackage("pack|nested.zip");
  CheckTrue("嵌套 zip 打开", nested != nullptr, "nested.zip 应解析为包");
  if (nested != nullptr) {
    CheckEq("嵌套 zip 内容",
            nested->GetStream("inner.txt").has_value() ? Str(*nested->GetStream("inner.txt"))
                                                       : "<null>",
            "INNER");
  }

  std::filesystem::remove_all(root);
}

void TestYamlLoad() {
  const auto root = MakeTempDir("yamlload");
  WriteText(root / "base.yaml", "^Base:\n\tKey: value\n");
  WriteText(root / "derived.yaml", "Actor:\n\tInherits: ^Base\n");
  WriteText(root / "extra.yaml", "Actor:\n\tExtra: yes\n");

  FileSystem fs;
  fs.Mount(root.generic_string());

  StringPool pool;

  const MiniYaml mapRules{pool.Intern("extra.yaml,  ,, extra.yaml"),
                          std::vector<MiniYamlNode>{}};

  auto merged = MiniYaml::Load(fs, std::vector<std::string>{"base.yaml", "derived.yaml"},
                               &mapRules, pool);
  CheckEq("Load 合并结果", WriteToString(merged),
          "^Base:\n\tKey: value\nActor:\n\tKey: value\n\tExtra: yes\n");

  std::filesystem::remove_all(root);
}

}

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);
  TestFolder();
  TestMountOrder();
  TestExplicitMount();
  TestZip();
  TestYamlLoad();

  std::println("fs_test: {} checks, {} failures", checks, failures);
  return failures == 0 ? 0 : 1;
}
