#pragma once
import std;

namespace ora::core {

struct Vector2 {
  float X{};
  float Y{};
  bool operator==(const Vector2&) const = default;
};

struct Vector3 {
  float X{};
  float Y{};
  float Z{};
  bool operator==(const Vector3&) const = default;
};

}
