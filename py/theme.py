
class Window:
    BG = 0x1e1e1e
    OUTLINE = 0x404040
    CLOSE_BUTTON = 0xFF1060
    MAX_BUTTON = 0x00FFFF
    MIN_BUTTON = 0x00ff00
    BUTTON_HOVER = 0x303030

    BUTTON_SIZE = 6
    OUTLINE_THICKNESS = 1

class Selector:
    BG = 0x242424
    TEXT = 0xEEEEEE
    BUTTON = 0x333333
    BUTTON_HOVER = 0x444444
    BUTTON_ACTIVE = 0x555555
    FONT_SIZE = 14
    BUTTON_PAD_WIDTH = 10
    BUTTON_PAD_HEIGHT = 10

    SIZEBAR_WIDTH = 5

class Terminal:
    BG = 0x2d2d2d
    TEXT = 0x96DED1
    CURSOR = 0xFFFF00
    LINE_NUMBER = 0xbbbbbb

    ANSI_COLORS = [
        0x1E1E1E,  # чёрный
        0xD72638,  # красный
        0x3EB049,  # зелёный
        0xF19D1A,  # жёлто-оранжевый
        0x1A6FF1,  # синий
        0xA347BA,  # фиолетовый
        0x20B2AA,  # бирюзовый
        0xC0C0C0   # светло-серый
    ]

    ANSI_BRIGHT_COLORS = [
        0x4B4B4B,  # серый
        0xFF5C57,  # мягкий алый
        0x5AF78E,  # светло-зелёный
        0xF3F99D,  # лимонно-жёлтый
        0x57C7FF,  # голубовато-синий
        0xFF6AC1,  # розово-фиолетовый
        0x9AEDFE,  # голубой
        0xFFFFFF   # белый
    ]
