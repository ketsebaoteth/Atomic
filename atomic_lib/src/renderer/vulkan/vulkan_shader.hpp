#pragma once
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace ui::vulkan::shader {
std::vector<char> readFile(const std::string &fileName);
}
