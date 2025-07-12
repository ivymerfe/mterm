#pragma once

#include <vector>
#include <optional>
#include <string>

namespace MTerm {

static class Utils {
 public:
  static std::vector<char32_t> Utf8ToUtf32(const char* utf8, size_t size);

  static std::vector<char> Utf32ToUtf8(const std::vector<char32_t>& utf32);

  static void Utf32CharToUtf8(char32_t codepoint, char out[4], int& out_len);

  static std::vector<std::vector<char32_t>> SplitByLines(
      const std::vector<char32_t>& input);

  static std::optional<std::string> GetFileContent(const char* filename);
};

}  // namespace MTerm
