#pragma once
import std;

#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "core/wangle.hpp"
#include "core/wpos.hpp"
#include "core/wrot.hpp"
#include "core/wvec.hpp"
#include "gfx/animation.hpp"
#include "gfx/renderable.hpp"
#include "meta/generic_record.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_interfaces.hpp"
#include "sim/type_dictionary.hpp"

namespace ora::game {
class ActorInfo;
}

namespace ora::gfx {

class WorldRenderer;

class IActorPreview {
 public:
  virtual ~IActorPreview() = default;
  virtual void Tick() = 0;
  virtual void Render(WorldRenderer& wr, WPos pos,
                      std::vector<RenderItem>& vec_out) = 0;
  virtual void RenderUI(WorldRenderer& wr, int2 pos, float scale,
                        std::vector<RenderItem>& vec_out) = 0;
  virtual void ScreenBounds(WorldRenderer& wr, WPos pos,
                            std::vector<Rectangle>& vec_out) = 0;
};

class ActorPreviewInitializer {
 public:
  ActorPreviewInitializer(const game::ActorInfo& actor, WorldRenderer& wr,
                          sim::TypeDictionary& dict)
      : actor_(actor), wr_(wr), dict_(dict) {}

  const game::ActorInfo& Actor() const { return actor_; }
  WorldRenderer& WR() { return wr_; }
  sim::World& World();
  sim::TypeDictionary& Dict() { return dict_; }

  template <class T>
  T* GetOrDefault(std::string_view info_instance_name) const {
    return sim::FindInitByInstanceName<T>(dict_, info_instance_name);
  }

  template <class T>
  T* Get(std::string_view info_instance_name) const {
    auto* init = GetOrDefault<T>(info_instance_name);
    if (init == nullptr)
      throw std::runtime_error(
          "TypeDictionary does not contain instance of type `" +
          std::string(ora::gen::kTypeFullNames[static_cast<std::size_t>(
              T::kTypeId)]) +
          "`");
    return init;
  }

  template <class T, class U>
  U GetValue(std::string_view info_instance_name) const {
    return Get<T>(info_instance_name)->Value();
  }

  template <class T, class U>
  U GetValue(std::string_view info_instance_name, U fallback) const {
    auto* init = GetOrDefault<T>(info_instance_name);
    return init != nullptr ? init->Value() : fallback;
  }

  template <class T>
  bool Contains(std::string_view info_instance_name) const {
    return GetOrDefault<T>(info_instance_name) != nullptr;
  }

  template <class T>
  T* GetOrDefault() const {
    return dict_.GetOrDefault<T>();
  }

  template <class T>
  T* Get() const {
    return dict_.Get<T>();
  }

  template <class T>
  auto GetValue() const {
    return Get<T>()->Value();
  }

  template <class T, class U>
  U GetValue(U fallback) const {
    auto* init = GetOrDefault<T>();
    return init != nullptr ? init->Value() : fallback;
  }

  template <class T>
  bool Contains() const {
    return GetOrDefault<T>() != nullptr;
  }

  std::function<WRot()> GetOrientation();
  std::function<WAngle()> GetFacing();
  sim::DamageState GetDamageState();

 private:
  const game::ActorInfo& actor_;
  WorldRenderer& wr_;
  sim::TypeDictionary& dict_;
};

class SpriteActorPreview final : public IActorPreview {
 public:
  SpriteActorPreview(Animation anim, std::function<WVec()> fn_offset,
                     std::function<int()> fn_z_offset,
                     const PaletteReference* ptr_palette)
      : anim_(std::move(anim)),
        fn_offset_(std::move(fn_offset)),
        fn_z_offset_(std::move(fn_z_offset)),
        ptr_palette_(ptr_palette) {}

  void Tick() override { anim_.Tick(); }

  void RenderUI(WorldRenderer& wr, int2 pos, float scale,
                std::vector<RenderItem>& vec_out) override;

  void Render(WorldRenderer& wr, WPos pos,
              std::vector<RenderItem>& vec_out) override;

  void ScreenBounds(WorldRenderer& wr, WPos pos,
                    std::vector<Rectangle>& vec_out) override;

 private:
  Animation anim_;
  std::function<WVec()> fn_offset_;
  std::function<int()> fn_z_offset_;
  const PaletteReference* ptr_palette_;
};

using ActorPreviewSpritesFn = std::function<void(
    const meta::RecordObject&, ActorPreviewInitializer&, const std::string&,
    int, PaletteReference*, std::vector<std::unique_ptr<IActorPreview>>&)>;

using RenderActorPreviewFn = std::function<void(
    const meta::RecordObject&, ActorPreviewInitializer&,
    std::vector<std::unique_ptr<IActorPreview>>&)>;

void RegisterActorPreviewSprites(std::string str_type_name,
                                 ActorPreviewSpritesFn fn);
void RegisterRenderActorPreview(std::string str_type_name,
                                RenderActorPreviewFn fn);

std::vector<std::pair<std::string, ActorPreviewSpritesFn>>&
ActorPreviewSpritesFaces();

std::vector<std::unique_ptr<IActorPreview>> RenderActorPreviewOf(
    const game::ActorInfo& actor, ActorPreviewInitializer& init);

}
