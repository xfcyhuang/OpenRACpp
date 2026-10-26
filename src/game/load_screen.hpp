// UPSTREAM: OpenRA.Game/ModData.cs @1f594e320 L236-252
#pragma once
import std;

#include "core/arguments.hpp"

namespace ora::fs {
class FileSystem;
}
namespace ora::game {
class Manifest;
class ModData;
}

namespace ora::game {

class ILoadScreen {
 public:
  virtual ~ILoadScreen() = default;

  virtual void Init(const Manifest& manifest,
                    const fs::FileSystem& file_system) = 0;
  virtual void Display() = 0;
  virtual bool BeforeLoad(ModData& mod_data) = 0;
  virtual void StartGame(const core::Arguments& args) = 0;
};

}  // namespace ora::game
