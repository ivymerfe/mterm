#pragma once

constexpr auto WINDOW_WIDTH = 900;
constexpr auto WINDOW_HEIGHT = 600;
constexpr auto WINDOW_MIN_WIDTH = 225;
constexpr auto WINDOW_MIN_HEIGHT = 150;

constexpr auto CAPTION_SIZE = 18;
constexpr auto BORDER_SIZE = 5;

constexpr auto SYS_BUTTON_SIZE = 25;
constexpr auto CLOSE_BUTTON_OFFSET = 5 + SYS_BUTTON_SIZE;
constexpr auto MAX_BUTTON_OFFSET = 40 + SYS_BUTTON_SIZE;
constexpr auto MIN_BUTTON_OFFSET = 75 + SYS_BUTTON_SIZE;

constexpr wchar_t FONT_NAME[] = L"Cascadia Mono";
constexpr int DEFAULT_FONT_SIZE = 11;

constexpr auto WINDOW_BG_COLOR = 0x191919;
constexpr auto CLOSE_BUTTON_COLOR = 0xff0000;
constexpr auto MAX_BUTTON_COLOR = 0x00ff88;
constexpr auto MIN_BUTTON_COLOR = 0x00ff00;

constexpr int TEXT_COLOR = 0x96DED1;
constexpr int CURSOR_COLOR = 0xFFFF00;
constexpr int LINE_NUMBER_COLOR = 0xbbbbbb;

constexpr float TEXT_BACKGROUND_OPACITY = 0.6;

static const int ANSI_COLORS[] = {
    0x1E1E1E,  // чёрный
    0xD72638,  // красный
    0x3EB049,  // зелёный
    0xF19D1A,  // жёлто-оранжевый
    0x1A6FF1,  // синий
    0xA347BA,  // фиолетовый
    0x20B2AA,  // бирюзовый
    0xC0C0C0   // светло-серый
};

static const int ANSI_BRIGHT_COLORS[] = {
    0x4B4B4B,  // серый
    0xFF5C57,  // мягкий алый
    0x5AF78E,  // светло-зелёный
    0xF3F99D,  // лимонно-жёлтый
    0x57C7FF,  // голубовато-синий
    0xFF6AC1,  // розово-фиолетовый
    0x9AEDFE,  // голубой
    0xFFFFFF   // белый
};
