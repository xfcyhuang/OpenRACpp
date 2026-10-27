import std;
#include "gfx/sequence_set.hpp"

namespace ora::gfx {

namespace {

constexpr std::string_view kAbstractActorPrefix = "^";

}

const std::vector<std::pair<std::string, std::unique_ptr<ISpriteSequence>>>* SequenceSet::FindImagesEntry(
    const std::string& str_image) const {
  for (const auto& [str_key, vec_sequences] : vec_images_)
    if (str_key == str_image)
      return &vec_sequences;
  return nullptr;
}

SequenceSet::SequenceSet(Deps deps, ISpriteSequenceLoader& loader_sequences, std::string str_tile_set,
                         const yaml::MiniYaml* yaml_additional_sequences)
    : str_tile_set_{std::move(str_tile_set)}, ptr_loader_{&loader_sequences} {

  ptr_sprite_cache_ = std::make_unique<SpriteCache>(*deps.ptr_file_system, deps.vec_sprite_loaders,
                                                    deps.int4_bgra_sheet_size,
                                                    deps.int4_indexed_sheet_size, 1, 1,
                                                    deps.ptr_render);

  vec_nodes_ = yaml::MiniYaml::Load(*deps.ptr_file_system, *deps.vec_sequence_files,
                                    yaml_additional_sequences, pool_yaml_);

  for (const yaml::MiniYamlNode& node : vec_nodes_) {
    const std::string str_key = node.Key != nullptr ? *node.Key : std::string{};
    if (str_key.starts_with(kAbstractActorPrefix))
      continue;

    auto vec_parsed = ptr_loader_->ParseSequences(*ptr_sprite_cache_, str_tile_set_, node);
    const auto it_existing =
        std::ranges::find_if(vec_images_, [&](const auto& kv) { return kv.first == str_key; });
    if (it_existing != vec_images_.end())
      it_existing->second = std::move(vec_parsed);
    else
      vec_images_.emplace_back(str_key, std::move(vec_parsed));
  }
}

ISpriteSequence& SequenceSet::GetSequence(const std::string& str_image,
                                          const std::string& str_sequence) const {
  const auto* vec_sequences = FindImagesEntry(str_image);
  if (vec_sequences == nullptr)
    throw std::runtime_error(
        std::format("Image `{}` does not have any sequences defined.", str_image));

  for (const auto& [str_name, seq_sequence] : *vec_sequences)
    if (str_name == str_sequence)
      return *seq_sequence;

  throw std::runtime_error(
      std::format("Image `{}` does not have a sequence named `{}`.", str_image, str_sequence));
}

std::vector<std::string> SequenceSet::Images() const {
  std::vector<std::string> vec_names;
  vec_names.reserve(vec_images_.size());
  for (const auto& [str_key, vec_ignored] : vec_images_)
    vec_names.push_back(str_key);
  return vec_names;
}

bool SequenceSet::HasSequence(const std::string& str_image, const std::string& str_sequence) const {
  const auto* vec_sequences = FindImagesEntry(str_image);
  if (vec_sequences == nullptr)
    throw std::runtime_error(
        std::format("Image `{}` does not have any sequences defined.", str_image));

  for (const auto& [str_name, seq_ignored] : *vec_sequences)
    if (str_name == str_sequence)
      return true;
  return false;
}

std::vector<std::string> SequenceSet::Sequences(const std::string& str_image) const {
  const auto* vec_sequences = FindImagesEntry(str_image);
  if (vec_sequences == nullptr)
    throw std::runtime_error(
        std::format("Image `{}` does not have any sequences defined.", str_image));

  std::vector<std::string> vec_names;
  vec_names.reserve(vec_sequences->size());
  for (const auto& [str_name, seq_ignored] : *vec_sequences)
    vec_names.push_back(str_name);
  return vec_names;
}

void SequenceSet::LoadSprites() {
  ptr_sprite_cache_->LoadReservations();
  for (auto& [str_image_ignored, vec_sequences] : vec_images_)
    for (auto& [str_name_ignored, seq_sequence] : vec_sequences)
      seq_sequence->ResolveSprites(*ptr_sprite_cache_);
}

}
