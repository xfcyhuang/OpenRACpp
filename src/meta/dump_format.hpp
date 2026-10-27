#pragma once
import std;

#include "meta/generic_record.hpp"

namespace ora::meta {

std::string FormatValue(const FieldDesc& desc, const GenericValue& val);

std::string FormatFloatNet(float fp4_v);

}
