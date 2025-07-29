
def hsv_to_rgb_hex(h, s, v):
    """
    Convert HSV to RGB hex (0xRRGGBB).
    h: Hue [0, 1]
    s: Saturation [0, 1]
    v: Value [0, 1]
    Returns: int (e.g., 0xFF00FF)
    """
    if s == 0.0:
        r = g = b = int(v * 255)
    else:
        i = int(h * 6)
        f = (h * 6) - i
        p = int(255 * v * (1 - s))
        q = int(255 * v * (1 - s * f))
        t = int(255 * v * (1 - s * (1 - f)))
        v = int(v * 255)
        i = i % 6
        if i == 0:
            r, g, b = v, t, p
        elif i == 1:
            r, g, b = q, v, p
        elif i == 2:
            r, g, b = p, v, t
        elif i == 3:
            r, g, b = p, q, v
        elif i == 4:
            r, g, b = t, p, v
        elif i == 5:
            r, g, b = v, p, q
        else:
            raise ValueError("Invalid hue value: {}".format(h))
    return (r << 16) | (g << 8) | b


def get_normal_color(index):
    """
    Generate a unique rainbow color for a button based on the index.
    Returns: int (0xRRGGBB)
    """
    # Spread hues evenly in [0, 1]
    h = (index * 0.15) % 1.0
    s = 0.5
    v = 0.5
    return hsv_to_rgb_hex(h, s, v)

def get_hovered_color(index):
    """
    Generate a hovered color similar to the base color.
    Returns: int (0xRRGGBB)
    """
    h = (index * 0.15) % 1.0
    s = 0.5
    v = 0.65
    return hsv_to_rgb_hex(h, s, v)

def get_active_color(index):
    """
    Generate an active color similar to the base color.
    Returns: int (0xRRGGBB)
    """
    h = (index * 0.15) % 1.0
    s = 0.5
    v = 0.75
    return hsv_to_rgb_hex(h, s, v)
