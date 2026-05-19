#include "vulkan_shader.hpp"
#include <fstream>

namespace ui::vulkan::shader {
std::vector<char> readFile(const std::string &fileName) {
  std::ifstream file(fileName, std::ios::ate | std::ios::binary);
  if (!file.is_open())
    throw std::runtime_error("failed to open shader file: " + fileName);
  size_t fileSize = (size_t)file.tellg();
  std::vector<char> buffer(fileSize);
  file.seekg(0);
  file.read(buffer.data(), fileSize);
  file.close();
  return buffer;
}

} // namespace ui::vulkan::shader
