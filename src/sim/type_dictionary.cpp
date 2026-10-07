// UPSTREAM: OpenRA.Game/Primitives/TypeDictionary.cs @b6fc03f(实现部分)
//          The implementation half of TypeDictionary.cs.
#include "sim/type_dictionary.hpp"

namespace ora::sim {

void TypeDictionary::Add(ActorInit* val) {
  // L41-53:按注册面全量登记(GetInterfaces()+BaseTypes() 的静态等价;
  // BaseTypes 含 self —— 具体类型键 GetInitTypeId 一并注册,上游
  // typeof(T) 容器查询的键源)
  // L41-53: register the whole face (the static equivalent of
  // GetInterfaces()+BaseTypes(); BaseTypes includes self — the concrete
  // type key GetInitTypeId registers too, the key source of upstream's
  // typeof(T) container queries).
  std::vector<gen::TypeId> keys{val->InitTypeIds().begin(),
                                val->InitTypeIds().end()};
  keys.push_back(val->GetInitTypeId());
  for (gen::TypeId t : keys) {
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
  // L98-116:逐键移除,空容器删键(键集与 Add 对称 —— 含 self 具体键)
  // L98-116: remove per key, erasing emptied buckets (the key set mirrors
  // Add's — the self concrete key included).
  std::vector<gen::TypeId> keys{val->InitTypeIds().begin(),
                                val->InitTypeIds().end()};
  keys.push_back(val->GetInitTypeId());
  for (gen::TypeId t : keys) {
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
