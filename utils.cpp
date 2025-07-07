#include "utils.h"

#include <stdexcept>

bool ReadFileContent(LPCWSTR file_path, std::vector<char>& buffer) {
  HANDLE file = CreateFile(file_path, GENERIC_READ, 0, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, 0);
  if (file == INVALID_HANDLE_VALUE) {
    return false;
  }

  LARGE_INTEGER fileSize;
  if (!GetFileSizeEx(file, &fileSize)) {
    CloseHandle(file);
    return false;
  }

  if (fileSize.QuadPart > MAXDWORD) {
    CloseHandle(file);
    return false;
  }
  buffer.resize(fileSize.QuadPart);

  DWORD bytesRead;
  if (!ReadFile(file, buffer.data(), static_cast<DWORD>(fileSize.QuadPart),
                &bytesRead, NULL) ||
      bytesRead != fileSize.QuadPart) {
    CloseHandle(file);
    return false;
  }
  CloseHandle(file);
  return true;
}

std::vector<char32_t> Utf8ToUtf32(const std::vector<char>& utf8) {
  std::vector<char32_t> utf32;
  size_t i = 0;

  while (i < utf8.size()) {
    uint32_t codepoint = 0;
    uint8_t byte = utf8[i];

    if ((byte & 0x80) == 0) {
      // 1-byte sequence
      codepoint = byte;
      i += 1;
    } else if ((byte & 0xE0) == 0xC0) {
      // 2-byte sequence
      if (i + 1 >= utf8.size())
        throw std::runtime_error("Truncated UTF-8 sequence");
      codepoint = ((byte & 0x1F) << 6) | (utf8[i + 1] & 0x3F);
      i += 2;
    } else if ((byte & 0xF0) == 0xE0) {
      // 3-byte sequence
      if (i + 2 >= utf8.size())
        throw std::runtime_error("Truncated UTF-8 sequence");
      codepoint = ((byte & 0x0F) << 12) | ((utf8[i + 1] & 0x3F) << 6) |
                  (utf8[i + 2] & 0x3F);
      i += 3;
    } else if ((byte & 0xF8) == 0xF0) {
      // 4-byte sequence
      if (i + 3 >= utf8.size())
        throw std::runtime_error("Truncated UTF-8 sequence");
      codepoint = ((byte & 0x07) << 18) | ((utf8[i + 1] & 0x3F) << 12) |
                  ((utf8[i + 2] & 0x3F) << 6) | (utf8[i + 3] & 0x3F);
      i += 4;
    } else {
      throw std::runtime_error("Invalid UTF-8 byte");
    }

    utf32.push_back(static_cast<char32_t>(codepoint));
  }

  return utf32;
}

std::vector<char> Utf32ToUtf8(const std::vector<char32_t>& utf32) {
  std::vector<char> utf8;
  for (char32_t codepoint : utf32) {
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
  return utf8;
}

void Utf32CharToUtf8(char32_t codepoint, char out[4], int& out_len) {
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
std::vector<std::vector<char32_t>> SplitByLines(
    const std::vector<char32_t>& input) {
  std::vector<std::vector<char32_t>> lines;
  std::vector<char32_t> current_line;

  size_t i = 0;
  while (i < input.size()) {
    char32_t c = input[i];

    if (c == U'\r') {  // Carriage Return
      if (i + 1 < input.size() && input[i + 1] == U'\n') {
        // Windows-style CRLF
        ++i;
      }
      lines.push_back(current_line);
      current_line.clear();
    } else if (c == U'\n') {  // Line Feed
      lines.push_back(current_line);
      current_line.clear();
    } else {
      current_line.push_back(c);
    }

    ++i;
  }

  // Add the last line if it's non-empty or if input ends with a newline
  if (!current_line.empty() ||
      (!input.empty() && (input.back() == U'\n' || input.back() == U'\r'))) {
    lines.push_back(current_line);
  }

  return lines;
}
