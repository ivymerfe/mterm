#include "Utils.h"

#include <stdexcept>
#include "Windows.h"

namespace MTerm {

void Utils::Utf8ToUtf32(const char* utf8,
                        size_t size,
                        std::vector<char32_t>& utf32) {
  size_t i = 0;

  while (i < size) {
    uint32_t codepoint = 0;
    uint8_t byte = utf8[i];

    if ((byte & 0x80) == 0) {
      // 1-byte sequence
      codepoint = byte;
      i += 1;
    } else if ((byte & 0xE0) == 0xC0) {
      // 2-byte sequence
      if (i + 1 >= size)
        throw std::runtime_error("Truncated UTF-8 sequence");
      codepoint = ((byte & 0x1F) << 6) | (utf8[i + 1] & 0x3F);
      i += 2;
    } else if ((byte & 0xF0) == 0xE0) {
      // 3-byte sequence
      if (i + 2 >= size)
        throw std::runtime_error("Truncated UTF-8 sequence");
      codepoint = ((byte & 0x0F) << 12) | ((utf8[i + 1] & 0x3F) << 6) |
                  (utf8[i + 2] & 0x3F);
      i += 3;
    } else if ((byte & 0xF8) == 0xF0) {
      // 4-byte sequence
      if (i + 3 >= size)
        throw std::runtime_error("Truncated UTF-8 sequence");
      codepoint = ((byte & 0x07) << 18) | ((utf8[i + 1] & 0x3F) << 12) |
                  ((utf8[i + 2] & 0x3F) << 6) | (utf8[i + 3] & 0x3F);
      i += 4;
    } else {
      throw std::runtime_error("Invalid UTF-8 byte");
    }

    utf32.push_back(static_cast<char32_t>(codepoint));
  }
}

void Utils::Utf32ToUtf8(const char32_t* utf32,
                        size_t length,
                        std::vector<char>& utf8) {
  for (int i = 0; i < length; i++) {
    char32_t codepoint = utf32[i];
    if (codepoint <= 0x7F) {
      // 1-byte sequence
      utf8.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
      // 2-byte sequence
      utf8.push_back(static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F)));
      utf8.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
      // 3-byte sequence
      utf8.push_back(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
      utf8.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
      utf8.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0x10FFFF) {
      // 4-byte sequence
      utf8.push_back(static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07)));
      utf8.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
      utf8.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
      utf8.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
      throw std::runtime_error("Invalid UTF-32 codepoint");
    }
  }
}

void Utils::Utf32CharToUtf8(char32_t codepoint, char out[4], int& out_len) {
  if (codepoint <= 0x7F) {
    out[0] = static_cast<char>(codepoint);
    out_len = 1;
  } else if (codepoint <= 0x7FF) {
    out[0] = static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F));
    out[1] = static_cast<char>(0x80 | (codepoint & 0x3F));
    out_len = 2;
  } else if (codepoint <= 0xFFFF) {
    out[0] = static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F));
    out[1] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
    out[2] = static_cast<char>(0x80 | (codepoint & 0x3F));
    out_len = 3;
  } else if (codepoint <= 0x10FFFF) {
    out[0] = static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07));
    out[1] = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
    out[2] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
    out[3] = static_cast<char>(0x80 | (codepoint & 0x3F));
    out_len = 4;
  } else {
    out_len = 0;
  }
}

}  // namespace MTerm
