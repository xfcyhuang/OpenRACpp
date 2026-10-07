// UPSTREAM: OpenRA.Mods.Common/Traits/Armor.cs 实现部分
//          The implementation half.
#include "mods/armor.hpp"

namespace ora::mods {

ArmorInfoData ArmorInfoData::Parse(const meta::RecordObject& rec_info) {
  ArmorInfoData data;
  if (const auto v = sim::RecordFieldString(rec_info, "Type"))
    data.str_type = std::string{*v};
  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  return data;
}

}  // namespace ora::mods
