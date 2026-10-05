// UPSTREAM: OpenRA.Mods.Cnc/FileSystem/MixFile.cs @7d57605 L24-248(逐语义重写)
// mix 包:三格式(C&C 无标志头 / RA-TS 带 u16 标志 / 加密头 = 80 字节
// 密钥块 RSA 解密 + Blowfish 块解密)+ 哈希 → 文件名解析(local mix
// database.dat 内嵌库 + global 库双哈希命中计数择优)。
// 形态适配(D 登记):字节全量驻留(GetStream 返回整读向量 = i_package
// 契约);条目序 = 上游 Dictionary/HashSet 序,本就非契约(进程随机),
// Contents 顺序同样不作承诺。
// The mix package: three formats (the C&C flagless header / the RA-TS u16
// flags / the encrypted header = the 80-byte keyblock RSA-decrypted +
// Blowfish block-decrypted) + hash → filename resolution (the embedded
// "local mix database.dat" + the global database, the better-scoring of the
// Classic/CRC32 hit counts). Shape adaptations (registered as deviations):
// bytes resident in full (GetStream returns whole-read vectors per the
// i_package contract); entry ordering mirrors the upstream Dictionary/
// HashSet orders, which were never contractual (process-randomized) —
// Contents order is likewise unspecified.
#pragma once
import std;

#include "fs/i_package.hpp"
#include "fs/package_entry.hpp"

namespace ora::fs {

/// MixFile(MixFile.cs L26-227 嵌套类):只读 mix 包。
/// MixFile (the MixFile.cs L26-227 nested class): the read-only mix package.
class MixFile final : public IReadOnlyPackage {
 public:
  /// 接管 bytes(C# ctor 接管 Stream;globalFilenames = global mix
  /// database.dat 的名字集,可为空)。
  /// Takes ownership of bytes (the C# ctor takes over the Stream;
  /// globalFilenames = the "global mix database.dat" name set, possibly
  /// empty).
  MixFile(std::vector<char> vec_bytes, std::string str_filename,
          std::span<const std::string> vec_global_filenames);
  ~MixFile() override = default;

  MixFile(const MixFile&) = delete;
  MixFile& operator=(const MixFile&) = delete;

  const std::string& Name() const override { return str_name_; }
  std::vector<std::string> Contents() const override;
  std::optional<std::vector<char>> GetStream(const std::string& str_filename) const override;
  bool Contains(const std::string& str_filename) const override;
  std::unique_ptr<IReadOnlyPackage> OpenPackage(const std::string& str_filename,
                                                FileSystem& context) const override;

  /// Index(L196-203):名字 → 绝对偏移条目(偏移 + dataStart)。顺序与
  /// Contents 同源(非契约)。
  /// Index (L196-203): name → absolute-offset entries (offset + dataStart).
  /// Ordering shares the Contents source (non-contractual).
  std::vector<std::pair<std::string, PackageEntry>> AbsoluteIndex() const;

  /// 调试面:格式检测产物(上游 ctor 局部状态的等价暴露)。
  /// Debug surface: the format-detection products (the equivalent exposure
  /// of the upstream ctor's locals).
  bool IsCncMix() const { return b_isCncMix_; }
  bool IsEncrypted() const { return b_isEncrypted_; }
  std::int64_t DataStart() const { return int8_dataStart_; }

 private:
  /// ParseIndex(L67-112):哈希表 + 内嵌 local 库 + 双哈希计数择优。
  /// ParseIndex (L67-112): the hash table + the embedded local database +
  /// the better-scoring of the two hash lookups.
  std::unordered_map<std::string, PackageEntry> ParseIndex(
      const std::unordered_map<std::uint32_t, PackageEntry>& map_entries,
      std::span<const std::string> vec_global_filenames);

  /// 条目内容区间(dataStart + offset 起 length 字节)。
  /// The entry content slice (length bytes from dataStart + offset).
  std::span<const char> ContentOf(const PackageEntry& entry) const;

  std::vector<char> vec_bytes_;
  std::string str_name_;
  std::unordered_map<std::string, PackageEntry> map_index_;
  std::int64_t int8_dataStart_ = 0;
  bool b_isCncMix_ = false;
  bool b_isEncrypted_ = false;
};

/// MixLoader(MixFile.cs L24-231):".mix" 后缀嗅探 + global 库惰性单次加载。
/// MixLoader (MixFile.cs L24-231): the ".mix" suffix sniff + the lazy
/// one-shot global-database load.
class MixLoader final : public IPackageLoader {
 public:
  bool TryParsePackage(std::span<const char> vec_bytes, const std::string& str_filename,
                       FileSystem& context,
                       std::unique_ptr<IReadOnlyPackage>& package) override;

 private:
  /// 全局名集(null = 未尝试加载;空 = 尝试过但无 global 库)。
  /// The global name set (null = not yet attempted; empty = attempted with
  /// no global database present).
  std::unique_ptr<std::vector<std::string>> ptr_globalFilenames_;
};

}  // namespace ora::fs
