#pragma once

#include <vector>
#include <optional>
#include <string>

namespace MTerm {

static class Utils {
 public:
  static void Utf8ToUtf32(const char* utf8, size_t size, std::vector<char32_t>& utf32);

  static void Utf32ToUtf8(const char32_t* utf32,
                   size_t length,
                   std::vector<char>& utf8);

  static void Utf32CharToUtf8(char32_t codepoint, char out[4], int& out_len);
};

}  // namespace MTerm
