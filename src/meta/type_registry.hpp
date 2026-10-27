#pragma once
import std;
#include "meta/field_desc.hpp"

namespace ora::meta {

class RecordObject {
 public:
  virtual ~RecordObject() = default;

  virtual const RecordDesc& record_desc() const = 0;
};

class TypeRegistry {
 public:

  static void Register(RecordDesc desc, std::function<std::unique_ptr<RecordObject>()> fn_factory);

  static const RecordDesc* FindType(std::string_view str_class_name);

  static std::unique_ptr<RecordObject> CreateObject(std::string_view str_class_name);

  static void ClearForTest();

  static std::vector<std::string_view> AllTypeNames();
};

}
