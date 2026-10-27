#pragma once
import std;

#include "meta/field_desc.hpp"
#include "meta/generic_record.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::meta {

std::string SaveFormatValue(const FieldDesc& desc, const GenericValue& val);

yaml::MiniYaml SaveRecord(const RecordObject& rec, yaml::StringPool& pool);

yaml::MiniYaml SaveRecordDifferences(const RecordObject& rec_o,
                                     const RecordObject& rec_from,
                                     yaml::StringPool& pool);

yaml::MiniYamlNode SaveRecordField(const RecordObject& rec,
                                   std::string_view str_field,
                                   yaml::StringPool& pool);

}
