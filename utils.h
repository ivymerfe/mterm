#pragma once

#include "Windows.h"

#include <vector>

bool ReadFileContent(LPCWSTR file_path, std::vector<char>& buffer);

std::vector<char32_t> Utf8ToUtf32(const std::vector<char>& utf8);

std::vector<char> Utf32ToUtf8(const std::vector<char32_t>& utf32);

void Utf32CharToUtf8(char32_t codepoint, char out[4], int& out_len);

std::vector<std::vector<char32_t>> SplitByLines(
    const std::vector<char32_t>& input);
