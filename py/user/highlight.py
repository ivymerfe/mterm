import re


def highlight(text, color, underline_color, background_color):
    # Post-process text
    match = re.match(r'^(PS\s+[A-Z]:\\.*?)', text)
    if match:
        color = 0x5AF78E
    return color, underline_color, background_color
