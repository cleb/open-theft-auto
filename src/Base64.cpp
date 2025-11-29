#include "Base64.hpp"

#include <array>

namespace {
const std::array<int, 256> kDecodingTable = [] {
    std::array<int, 256> table{};
    table.fill(-1);
    const std::string alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (std::size_t i = 0; i < alphabet.size(); ++i) {
        table[static_cast<unsigned char>(alphabet[i])] = static_cast<int>(i);
    }
    table[static_cast<unsigned char>('=')] = 0;
    return table;
}();

const std::array<char, 64> kEncodingTable = [] {
    std::array<char, 64> table{};
    const std::string alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (std::size_t i = 0; i < alphabet.size(); ++i) {
        table[i] = alphabet[i];
    }
    return table;
}();
}

namespace base64 {

std::vector<unsigned char> decode(const std::string& input) {
    std::vector<unsigned char> output;
    output.reserve((input.size() * 3) / 4);

    int value = 0;
    int bits = -8;

    for (unsigned char c : input) {
        int decoded = kDecodingTable[c];
        if (decoded == -1) {
            continue;  // Skip whitespace or invalid characters silently
        }
        value = (value << 6) + decoded;
        bits += 6;

        if (bits >= 0) {
            output.push_back(static_cast<unsigned char>((value >> bits) & 0xFF));
            bits -= 8;
        }
    }

    return output;
}

std::string encode(const std::vector<unsigned char>& input) {
    std::string output;
    output.reserve(((input.size() + 2) / 3) * 4);

    std::size_t i = 0;
    while (i < input.size()) {
        int value = 0;
        int count = 0;
        for (; count < 3 && i < input.size(); ++count, ++i) {
            value = (value << 8) + input[i];
        }

        value <<= (3 - count) * 8;

        output.push_back(kEncodingTable[(value >> 18) & 0x3F]);
        output.push_back(kEncodingTable[(value >> 12) & 0x3F]);
        output.push_back(count >= 2 ? kEncodingTable[(value >> 6) & 0x3F] : '=');
        output.push_back(count == 3 ? kEncodingTable[value & 0x3F] : '=');
    }

    return output;
}

}  // namespace base64
