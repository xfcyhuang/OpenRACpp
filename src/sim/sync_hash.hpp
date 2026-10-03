// UPSTREAM: OpenRA.Game/Sync.cs @7d57605 L23-211(逐语义重写;反射 Emit 的
//          哈希生成协议 → 编译期哈希函数族 + gen/sync_gen.cpp 成员表)
//          Verbatim-semantics rewrite; the Reflection.Emit hash generation
//          protocol becomes the compile-time hash function family + the
//          gen/sync_gen.cpp member table.
//
// 机制对照 / Mechanism mapping:
//  - C# GenerateHashFunc(Sync.cs L78-107):Reflection.Emit 生成
//    "0 XOR hash(f1) XOR hash(f2) …"(GetFields 序先、GetProperties 序后,
//    Public|NonPublic|Instance)→ C++ 侧 CombineSyncHash 起始 0 的 XOR 链;
//    成员集由 gen/sync_gen.cpp 注册表锚定(Phase 5 手写 trait 按表接线)
//    C# GenerateHashFunc emits "0 XOR hash(f1) XOR hash(f2) …" (GetFields
//    order first, GetProperties after, Public|NonPublic|Instance) → the
//    C++ CombineSyncHash XOR chain seeded with 0; member sets are anchored
//    by the gen/sync_gen.cpp registry (Phase 5 hand-written traits wire up
//    against the table).
//  - bool 字段哈希(EmitSyncOpcodes L63-71):IL 恒真跳转使 Pop/0x555 分支
//    不可达,实际语义 = (b ? 1 : 0) ^ 0xAAA → HashBool
//    bool member hash (EmitSyncOpcodes L63-71): the IL branch is
//    unconditionally taken, leaving Pop/0x555 unreachable; the live
//    semantics are (b ? 1 : 0) ^ 0xAAA → HashBool.
//  - HashUsingHashCode(WDist/WPos/WVec/WAngle/WRot):C# GetHashCode 为值
//    XOR 组合(int.GetHashCode = 原值)→ 下方 HashUsingHashCode 家族
//    HashUsingHashCode (WDist/WPos/WVec/WAngle/WRot): the C# GetHashCode
//    is a value XOR combination (int.GetHashCode is identity) → the
//    HashUsingHashCode family below.
//  - RunUnsynced/AssertUnsynced(L166-211):unsyncCount 重入门禁 + 世界哈希
//    前后校验;世界指针空判定语义保留(disposing 世界跳过)
//    RunUnsynced/AssertUnsynced (L166-211): the unsyncCount reentry gate +
//    world-hash before/after check; the null-world semantics are kept
//    (disposing worlds skip the check).
#pragma once
import std;

#include "core/cell_pos.hpp"
#include "core/int2.hpp"
#include "core/wangle.hpp"
#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "core/wrot.hpp"
#include "core/wvec.hpp"
#include "core/cvec.hpp"
#include "gen/interfaces_gen.h"

namespace ora::sim {

class Actor;
class Player;
class World;
struct Target;

/// ISync 标记接口(Sync.cs L27;实现类的 [VerifySync] 成员参与 SyncHash)
/// The ISync marker interface (Sync.cs L27; a implementing type's
/// [VerifySync] members join the SyncHash).
class ISync {
 public:
  static constexpr gen::TypeId kTypeId = gen::TypeId::OpenRA_ISync;
  virtual ~ISync() = default;
};

/// gen/sync_gen.cpp 的成员描述([VerifySync] 标注成员;Phase 5 手写 trait
/// 哈希接线的权威字段集)
/// The member descriptor for gen/sync_gen.cpp ([VerifySync]-marked members;
/// the authoritative field set Phase 5 hand-written trait hashes wire to).
struct SyncMemberDesc {
  std::string_view str_name;
  bool b_is_property;
  std::string_view str_type_name;        // TypeSignature 形态(TypeSignature form)
  std::string_view str_declaring_type;   // 声明类型全名(declaring type full name)
};

namespace sync {

// ———— 哈希函数族(Sync.cs CustomHashFunctions + 内建;全部 int 语义,-fwrapv)————
// ———— The hash function family (Sync.cs CustomHashFunctions + built-ins;
//      all int arithmetic under -fwrapv) ————

/// HashInt2(Sync.cs L109-112)
inline int HashInt2(const int2& i2) {
  return ((i2.X * 5) ^ (i2.Y * 3)) / 4;
}

/// HashCPos(Sync.cs L114-117;CPos.GetHashCode = Bits)
/// HashCPos (Sync.cs L114-117; CPos.GetHashCode = Bits).
inline int HashCPos(const CPos& p) {
  return p.Bits;
}

/// HashCVec(Sync.cs L119-122)
inline int HashCVec(const CVec& v) {
  return ((v.X * 5) ^ (v.Y * 3)) / 4;
}

/// HashActor(Sync.cs L124-129;定义在 actor.hpp——此处仅声明)
/// HashActor (Sync.cs L124-129; defined in actor.hpp — declared here).
int HashActor(const Actor* a);

/// HashPlayer(Sync.cs L131-136;定义在 player.hpp)
/// HashPlayer (Sync.cs L131-136; defined in player.hpp).
int HashPlayer(const Player* p);

/// HashTarget(Sync.cs L138-159;定义在 target.hpp)
/// HashTarget (Sync.cs L138-159; defined in target.hpp).
int HashTarget(const Target& t);

/// HashUsingHashCode 家族(WDist/WAngle/WPos/WVec/WRot 的 C# GetHashCode)
/// The HashUsingHashCode family (the C# GetHashCode of
/// WDist/WAngle/WPos/WVec/WRot).
inline int HashWDist(const WDist& d) { return d.Length; }
inline int HashWAngle(const WAngle& a) { return a.Angle; }
inline int HashWPos(const WPos& p) {
  return p.X ^ p.Y ^ p.Z;
}
inline int HashWVec(const WVec& v) {
  return v.X ^ v.Y ^ v.Z;
}
inline int HashWRot(const WRot& r) {
  return r.Roll.Angle ^ r.Pitch.Angle ^ r.Yaw.Angle;
}

/// bool 字段哈希(EmitSyncOpcodes 的可达语义:值 0/1 XOR 0xAAA)
/// bool member hash (the reachable semantics of EmitSyncOpcodes:
/// the 0/1 value XOR 0xAAA).
inline int HashBool(bool b) {
  return (b ? 1 : 0) ^ 0xAAA;
}

/// GenerateHashFunc 的组合协议:acc 从 0 起,逐成员 XOR(调用方按
/// gen/sync_gen.cpp 表序贡献;int 成员直接传值)
/// The GenerateHashFunc combination protocol: acc starts at 0 and XORs each
/// member contribution in order (callers contribute following the
/// gen/sync_gen.cpp table order; int members pass their value directly).
inline int CombineSyncHash(int acc, int member_hash) {
  return acc ^ member_hash;
}

}  // namespace sync

// ———— 注册表协议(gen/sync_gen.cpp 与 Phase 5 手写类的接线面)————
// ———— The registry protocol (the wiring surface of gen/sync_gen.cpp and
//      Phase 5 hand-written classes) ————

/// ISync 类型哈希函数形态(GenerateHashFunc 产物的 C++ 等价)
/// The shape of an ISync type's hash function (the C++ equivalent of the
/// GenerateHashFunc product).
using SyncHashFn = int (*)(const ISync*);

void RegisterSyncMembers(std::string_view str_type_full_name,
                         std::span<const SyncMemberDesc> members);

std::span<const SyncMemberDesc> FindSyncMembers(
    std::string_view str_type_full_name);

void RegisterSyncHashFunction(std::string_view str_type_full_name,
                              SyncHashFn fn);

SyncHashFn FindSyncHashFunctionByName(std::string_view str_type_full_name);

/// trait TypeId → 哈希函数(经全名反查;未注册 → nullptr,哈希贡献 0)
/// trait TypeId → hash function (via full-name lookup; nullptr when
/// unregistered — the hash contributes 0).
SyncHashFn FindSyncHashFunction(gen::TypeId trait_type_id);

// ———— AssertUnsynced(Sync.cs L206-211;RunUnsynced 见 world.hpp 尾部
//      —— 模板体需 World 完整类型)————
// ———— AssertUnsynced (Sync.cs L206-211; RunUnsynced lives at the end of
//      world.hpp — the template body needs the complete World type) ————

void AssertUnsynced(std::string_view message);

/// unsync 计数(AssertUnsynced/RunUnsynced 共享;测试用)
/// The unsync count (shared by AssertUnsynced/RunUnsynced; for tests).
int UnsyncCountForTest();

/// RunUnsynced 门禁实现访问(world.hpp 的模板用)
/// Access for the RunUnsynced gate implementation (used by the template
/// in world.hpp).
int& UnsyncCountRef();

}  // namespace ora::sim
