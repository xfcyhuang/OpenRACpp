// UPSTREAM: OpenRA.Game/Primitives/TypeDictionary.cs @b6fc03f(实现部分)
//          The implementation half of TypeDictionary.cs.
#include "sim/type_dictionary.hpp"

namespace ora::sim {

void TypeDictionary::Add(ActorInit* val) {
  // L41-53:按注册面全量登记(GetInterfaces()+BaseTypes() 的静态等价)
  for (gen::TypeId t : val->InitTypeIds()) {
    auto& bucket = data_[t];
    // 同键重复登记防重(C# 每接口一个容器只 Add 一次 —— 注册面元素唯一)
    bool present = false;
    for (void* p : bucket)
      present = present || p == val;
    if (!present)
      bucket.push_back(val);
  }
}

void TypeDictionary::Remove(ActorInit* val) {
  // L98-116:逐键移除,空容器删键
  for (gen::TypeId t : val->InitTypeIds()) {
    auto it = data_.find(t);
    if (it == data_.end())
      continue;

    auto& bucket = it->second;
    bucket.erase(std::remove(bucket.begin(), bucket.end(), val),
                 bucket.end());
    if (bucket.empty())
      data_.erase(it);
  }
}

}  // namespace ora::sim
