#pragma once

#include <string>
#include <vector>

namespace base64 {
std::vector<unsigned char> decode(const std::string& input);
std::string encode(const std::vector<unsigned char>& input);
}
