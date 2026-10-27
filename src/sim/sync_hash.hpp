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

class ISync {
 public:
  static constexpr gen::TypeId kTypeId = gen::TypeId::OpenRA_ISync;
  virtual ~ISync() = default;
};

struct SyncMemberDesc {
  std::string_view str_name;
  bool b_is_property;
  std::string_view str_type_name;
  std::string_view str_declaring_type;
};

namespace sync {

inline int HashInt2(const int2& i2) {
  return ((i2.X * 5) ^ (i2.Y * 3)) / 4;
}

inline int HashCPos(const CPos& p) {
  return p.Bits;
}

inline int HashCVec(const CVec& v) {
  return ((v.X * 5) ^ (v.Y * 3)) / 4;
}

int HashActor(const Actor* a);

int HashPlayer(const Player* p);

int HashTarget(const Target& t);

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

inline int HashBool(bool b) {
  return b ? 1 : 0;
}

inline int CombineSyncHash(int acc, int member_hash) {
  return acc ^ member_hash;
}

}

using SyncHashFn = int (*)(const ISync*);

void RegisterSyncMembers(std::string_view str_type_full_name,
                         std::span<const SyncMemberDesc> members);

std::span<const SyncMemberDesc> FindSyncMembers(
    std::string_view str_type_full_name);

void RegisterSyncHashFunction(std::string_view str_type_full_name,
                              SyncHashFn fn);

SyncHashFn FindSyncHashFunctionByName(std::string_view str_type_full_name);

SyncHashFn FindSyncHashFunction(gen::TypeId trait_type_id);

void AssertUnsynced(std::string_view message);

int UnsyncCountForTest();

int& UnsyncCountRef();

}
