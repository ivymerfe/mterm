#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "ColoredTextBuffer.h"
#include "PseudoConsole.h"
#include "Utils.h"
#include "Window.h"

namespace py = pybind11;

PYBIND11_MODULE(mterm, m) {
  m.doc() = "MTerm - Terminal emulator module";

  // Экспорт структур
  py::class_<LineFragment>(m, "LineFragment")
      .def(py::init<>())
      .def_readwrite("pos", &LineFragment::pos)
      .def_readwrite("color", &LineFragment::color)
      .def_readwrite("underline_color", &LineFragment::underline_color)
      .def_readwrite("background_color", &LineFragment::background_color);

  py::class_<ColoredLine>(m, "ColoredLine")
      .def(py::init<>())
      .def_property(
          "text",
          [](const ColoredLine& self) {
            // UTF-32 -> UTF-8 для чтения
            std::vector<char> utf8;
            MTerm::Utils::Utf32ToUtf8(self.text.data(), self.text.size(), utf8);
            return std::string(utf8.begin(), utf8.end());
          },
          [](ColoredLine& self, const std::string& utf8_str) {
            // UTF-8 -> UTF-32 для записи
            self.text.clear();
            MTerm::Utils::Utf8ToUtf32(utf8_str.c_str(), utf8_str.size(),
                                      self.text);
          })
      .def_readwrite("fragments", &ColoredLine::fragments);

  // Экспорт Config структуры с UTF-8 callback
  py::class_<MTerm::Config>(m, "Config")
      .def(py::init<>())
      .def_property(
          "font_name",
          [](const MTerm::Config& self) -> std::string {
            return MTerm::Utils::WCharToUtf8(self.font_name);
          },
          [](MTerm::Config& self, const std::string& utf8_str) {
            self.font_name = MTerm::Utils::Utf8ToWChar(utf8_str);
          })
      .def_readwrite("window_width", &MTerm::Config::window_width)
      .def_readwrite("window_height", &MTerm::Config::window_height)
      .def_readwrite("window_min_width", &MTerm::Config::window_min_width)
      .def_readwrite("window_min_height", &MTerm::Config::window_min_height)
      .def_readwrite("caption_size", &MTerm::Config::caption_size)
      .def_readwrite("border_size", &MTerm::Config::border_size)
      .def_readwrite("button_size", &MTerm::Config::button_size)
      .def_readwrite("close_button_offset", &MTerm::Config::close_button_offset)
      .def_readwrite("max_button_offset", &MTerm::Config::max_button_offset)
      .def_readwrite("min_button_offset", &MTerm::Config::min_button_offset)
      .def_readwrite("cursor_id", &MTerm::Config::cursor_id)
      .def_readwrite("render_callback", &MTerm::Config::render_callback)
      .def_readwrite("resize_callback", &MTerm::Config::resize_callback)
      .def_readwrite("keydown_callback", &MTerm::Config::keydown_callback)
      .def_readwrite("keyup_callback", &MTerm::Config::keyup_callback)
      .def_property(
          "input_callback",
          [](const MTerm::Config& self) { return self.input_callback; },
          [](MTerm::Config& self,
             std::function<void(const std::string&)> py_callback) {
            if (py_callback) {
              self.input_callback = [py_callback](char32_t chr) {
                char utf8_buf[4];
                int len;
                MTerm::Utils::Utf32CharToUtf8(chr, utf8_buf, len);
                if (len > 0) {
                  std::string utf8_str(utf8_buf, len);
                  py_callback(utf8_str);
                }
              };
            } else {
              self.input_callback = nullptr;
            }
          })
      .def_readwrite("mousemove_callback", &MTerm::Config::mousemove_callback)
      .def_readwrite("mousedown_callback", &MTerm::Config::mousedown_callback)
      .def_readwrite("mouseup_callback", &MTerm::Config::mouseup_callback)
      .def_readwrite("scroll_callback", &MTerm::Config::scroll_callback);

  // Экспорт PseudoConsole с UTF-8 callback
  py::class_<MTerm::PseudoConsole>(m, "PseudoConsole")
      .def(py::init<>())
      .def(
          "start",
          [](MTerm::PseudoConsole& self, short num_rows, short num_columns,
             std::function<void(const std::string&)> py_callback) {
            // Конвертируем Python UTF-8 callback в C++ callback
            auto wrapped_callback = [py_callback](const char* data,
                                                  unsigned int length) {
              py::gil_scoped_acquire acquire;
              std::string utf8_str(data, length);
              py_callback(utf8_str);
            };

            bool result;
            {
              py::gil_scoped_release release;
              result = self.Start(num_rows, num_columns, wrapped_callback);
            }
            return result;
          },
          "Start pseudo console", py::arg("num_rows"), py::arg("num_columns"),
          py::arg("callback"))
      .def(
          "send",
          [](MTerm::PseudoConsole& self, const std::string& utf8_data) {
            py::gil_scoped_release release;
            return self.Send(utf8_data.c_str(),
                             static_cast<unsigned int>(utf8_data.size()));
          },
          "Send data to console", py::arg("data"))
      .def(
          "resize",
          [](MTerm::PseudoConsole& self, short num_rows, short num_columns) {
            py::gil_scoped_release release;
            self.Resize(num_rows, num_columns);
          },
          "Resize console", py::arg("num_rows"), py::arg("num_columns"))
      .def(
          "close",
          [](MTerm::PseudoConsole& self) {
            py::gil_scoped_release release;
            self.Close();
          },
          "Close console");

  // Экспорт ColoredTextBuffer с UTF-8 интерфейсом
  py::class_<MTerm::ColoredTextBuffer>(m, "ColoredTextBuffer")
      .def(py::init<>())
      .def("add_line", &MTerm::ColoredTextBuffer::AddLine, "Add new line")
      .def("get_lines", &MTerm::ColoredTextBuffer::GetLines,
           py::return_value_policy::reference_internal, "Get all lines")
      .def(
          "write_to_line",
          [](MTerm::ColoredTextBuffer& self, size_t line_index,
             const std::string& utf8_text) {
            std::vector<char32_t> utf32;
            MTerm::Utils::Utf8ToUtf32(utf8_text.c_str(), utf8_text.size(),
                                      utf32);
            self.WriteToLine(line_index, utf32.data(),
                             static_cast<int>(utf32.size()));
          },
          "Write text to line", py::arg("line_index"), py::arg("text"))
      .def(
          "set_text",
          [](MTerm::ColoredTextBuffer& self, size_t line_index, int offset,
             const std::string& utf8_content) {
            std::vector<char32_t> utf32_content;
            MTerm::Utils::Utf8ToUtf32(utf8_content.c_str(), utf8_content.size(),
                                      utf32_content);
            self.SetText(line_index, offset, utf32_content.data(),
                         static_cast<int>(utf32_content.size()));
          },
          "Set text at position", py::arg("line_index"), py::arg("offset"),
          py::arg("content"))
      .def("set_color", &MTerm::ColoredTextBuffer::SetColor,
           "Set color for text range", py::arg("line_index"),
           py::arg("start_pos"), py::arg("end_pos"), py::arg("color"),
           py::arg("underline_color"), py::arg("background_color"));

  // Экспорт Window с UTF-8 интерфейсом
  py::class_<MTerm::Window>(m, "Window")
      .def(py::init<>())
      .def(
          "create",
          [](MTerm::Window& self, MTerm::Config config) {
            // Оборачиваем все callback для правильной работы с GIL
            if (config.render_callback) {
              auto original_render = config.render_callback;
              config.render_callback = [original_render]() {
                py::gil_scoped_acquire acquire;
                original_render();
              };
            }

            if (config.resize_callback) {
              auto original_resize = config.resize_callback;
              config.resize_callback = [original_resize](int width,
                                                         int height) {
                py::gil_scoped_acquire acquire;
                original_resize(width, height);
              };
            }

            if (config.keydown_callback) {
              auto original_keydown = config.keydown_callback;
              config.keydown_callback = [original_keydown](
                                            int keycode, bool ctrl, bool shift,
                                            bool alt) {
                py::gil_scoped_acquire acquire;
                original_keydown(keycode, ctrl, shift, alt);
              };
            }

            if (config.keyup_callback) {
              auto original_keyup = config.keyup_callback;
              config.keyup_callback = [original_keyup](int keycode, bool ctrl,
                                                       bool shift, bool alt) {
                py::gil_scoped_acquire acquire;
                original_keyup(keycode, ctrl, shift, alt);
              };
            }

            // input_callback уже обернут в Config property выше

            if (config.mousemove_callback) {
              auto original_mousemove = config.mousemove_callback;
              config.mousemove_callback = [original_mousemove](int x, int y) {
                py::gil_scoped_acquire acquire;
                original_mousemove(x, y);
              };
            }

            if (config.mousedown_callback) {
              auto original_mousedown = config.mousedown_callback;
              config.mousedown_callback = [original_mousedown](int button,
                                                               int x, int y) {
                py::gil_scoped_acquire acquire;
                original_mousedown(button, x, y);
              };
            }

            if (config.mouseup_callback) {
              auto original_mouseup = config.mouseup_callback;
              config.mouseup_callback = [original_mouseup](int button, int x,
                                                           int y) {
                py::gil_scoped_acquire acquire;
                original_mouseup(button, x, y);
              };
            }

            if (config.scroll_callback) {
              auto original_scroll = config.scroll_callback;
              config.scroll_callback = [original_scroll](int delta, int x,
                                                         int y) {
                py::gil_scoped_acquire acquire;
                original_scroll(delta, x, y);
              };
            }

            int result;
            {
              py::gil_scoped_release release;
              result = self.Create(config);
            }
            return result;
          },
          "Create window", py::arg("config"))
      .def(
          "destroy",
          [](MTerm::Window& self) {
            py::gil_scoped_release release;
            self.Destroy();
          },
          "Destroy window")
      .def(
          "set_cursor",
          [](MTerm::Window& self, int cursor_id) {
            py::gil_scoped_release release;
            self.SetCursor(cursor_id);
          },
          "Set cursor", py::arg("cursor_id"))
      .def(
          "redraw",
          [](MTerm::Window& self) {
            py::gil_scoped_release release;
            self.Redraw();
          },
          "Redraw window")
      .def(
          "resize",
          [](MTerm::Window& self, unsigned int width, unsigned int height) {
            py::gil_scoped_release release;
            self.Resize(width, height);
          },
          "Resize window", py::arg("width"), py::arg("height"))
      .def("get_width", &MTerm::Window::GetWidth, "Get window width")
      .def("get_height", &MTerm::Window::GetHeight, "Get window height")
      .def(
          "clear",
          [](MTerm::Window& self, int color) {
            py::gil_scoped_release release;
            self.Clear(color);
          },
          "Clear window", py::arg("color"))
      .def(
          "text",
          [](MTerm::Window& self, const std::string& utf8_text, float font_size,
             float x, float y, int color, int underline_color,
             int background_color, float opacity) {
            std::vector<char32_t> utf32;
            MTerm::Utils::Utf8ToUtf32(utf8_text.c_str(), utf8_text.size(),
                                      utf32);
            py::gil_scoped_release release;
            self.Text(utf32.data(), static_cast<int>(utf32.size()), font_size,
                      x, y, color, underline_color, background_color, opacity);
          },
          "Draw text", py::arg("text"), py::arg("font_size"), py::arg("x"),
          py::arg("y"), py::arg("color"), py::arg("underline_color") = -1,
          py::arg("background_color") = -1, py::arg("opacity") = 1.0f)
      .def(
          "line",
          [](MTerm::Window& self, float start_x, float start_y, float end_x,
             float end_y, float thickness, int color, float opacity) {
            py::gil_scoped_release release;
            self.Line(start_x, start_y, end_x, end_y, thickness, color,
                      opacity);
          },
          "Draw line", py::arg("start_x"), py::arg("start_y"), py::arg("end_x"),
          py::arg("end_y"), py::arg("thickness"), py::arg("color"),
          py::arg("opacity") = 1.0f)
      .def(
          "rect",
          [](MTerm::Window& self, float left, float top, float right,
             float bottom, int color, float opacity) {
            py::gil_scoped_release release;
            self.Rect(left, top, right, bottom, color, opacity);
          },
          "Draw rectangle", py::arg("left"), py::arg("top"), py::arg("right"),
          py::arg("bottom"), py::arg("color"), py::arg("opacity") = 1.0f)
      .def(
          "outline",
          [](MTerm::Window& self, float left, float top, float right,
             float bottom, float thickness, int color, float opacity) {
            py::gil_scoped_release release;
            self.Outline(left, top, right, bottom, thickness, color, opacity);
          },
          "Draw outline", py::arg("left"), py::arg("top"), py::arg("right"),
          py::arg("bottom"), py::arg("thickness"), py::arg("color"),
          py::arg("opacity") = 1.0f)
      .def(
          "text_buffer",
          [](MTerm::Window& self, MTerm::ColoredTextBuffer* buffer, float left,
             float top, float width, float height, int x_offset_chars,
             int y_offset_lines, float font_size) {
            py::gil_scoped_release release;
            self.TextBuffer(buffer, left, top, width, height, x_offset_chars,
                            y_offset_lines, font_size);
          },
          "Draw text buffer", py::arg("buffer"), py::arg("left"),
          py::arg("top"), py::arg("width"), py::arg("height"),
          py::arg("x_offset_chars"), py::arg("y_offset_lines"),
          py::arg("font_size"))
      .def("get_advance", &MTerm::Window::GetAdvance, "Get character advance",
           py::arg("font_size"))
      .def("get_line_width", &MTerm::Window::GetLineWidth, "Get line width",
           py::arg("font_size"), py::arg("num_chars"))
      .def("get_line_height", &MTerm::Window::GetLineHeight, "Get line height",
           py::arg("font_size"));

  // Константы
  m.attr("PTY_BUFFER_SIZE") = MTerm::PTY_BUFFER_SIZE;
  m.attr("TEXT_BUFFER_SIZE") = MTerm::TEXT_BUFFER_SIZE;
}