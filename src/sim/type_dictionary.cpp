#include "sim/type_dictionary.hpp"

namespace ora::sim {

void TypeDictionary::Add(ActorInit* val) {

  std::vector<gen::TypeId> keys{val->InitTypeIds().begin(),
                                val->InitTypeIds().end()};
  keys.push_back(val->GetInitTypeId());
  for (gen::TypeId t : keys) {
    auto& bucket = data_[t];

    bool present = false;
    for (void* p : bucket)
      present = present || p == val;
    if (!present)
      bucket.push_back(val);
  }
}

void TypeDictionary::Remove(ActorInit* val) {

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

  map_owned_.erase(val);
}

}
