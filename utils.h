#pragma once

#include <vector>

namespace MTerm {

static class Utils {
 public:
  static std::vector<char32_t> Utf8ToUtf32(const std::vector<char>& utf8);

  static std::vector<char> Utf32ToUtf8(const std::vector<char32_t>& utf32);

  static void Utf32CharToUtf8(char32_t codepoint, char out[4], int& out_len);

  static std::vector<std::vector<char32_t>> SplitByLines(
      const std::vector<char32_t>& input);
};

}  // namespace MTerm
