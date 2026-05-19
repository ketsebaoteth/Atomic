#pragma once

#include <vulkan/vulkan_core.h>
namespace math {
template <typename T> struct vec2 {
  T x, y;
  template <typename U> operator vec2<U>() const {
    return {static_cast<U>(x), static_cast<U>(y)};
  }

  operator VkExtent2D() const {
    return {static_cast<uint32_t>(x), static_cast<uint32_t>(y)};
  }
};
template <typename T> struct vec3 {
  T x, y, z;
};
template <typename T> struct vec4 {
  T x, y, z, w;
  template <typename U> operator vec4<U>() const {
    return {static_cast<U>(x), static_cast<U>(y), static_cast<U>(z),
            static_cast<U>(w)};
  }
};
} // namespace math
