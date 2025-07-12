
foreign class Renderer {
  construct new() {}

  foreign clear(color)
  foreign line(x1, y1, x2, y2, thickness, color, opacity)
  foreign rect(left, top, right, bottom, color, opacity)
  foreign outline(left, top, right, bottom, thickness, color, opacity)
  foreign text(text, fontSize, x, y, color, underlineColor, backgroundColor, opacity)
}
