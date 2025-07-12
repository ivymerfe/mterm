#pragma once

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "wren/wren.hpp"

#include "ColoredTextBuffer.h"
#include "Renderer.h"

namespace MTerm {

class MTerm {
 public:
  MTerm();

  void Init(void* window);
  void Destroy();

  bool IsInitialized();

  static WrenLoadModuleResult LoadModule(WrenVM* vm, const char* name);

  static WrenForeignClassMethods BindForeignClass(WrenVM* vm,
                                                  const char* module,
                                                  const char* className);

  static WrenForeignMethodFn BindForeignMethod(WrenVM* vm,
                                               const char* module,
                                               const char* className,
                                               bool isStatic,
                                               const char* signature);

  void Redraw();
  void Resize(unsigned int width, unsigned int height);
  void Render();

  void KeyDown(int key_code);
  void KeyUp(int key_code);
  void Input(char32_t key);

  void MouseMove(int x, int y);
  void MouseDown(int x, int y, int button);
  void MouseUp(int x, int y, int button);
  void Scroll(int x, int y, int delta);

 private:
  bool m_isInitialized = false;

  void* m_window;
  unsigned int m_width;
  unsigned int m_height;

  Renderer m_renderer;
  ColoredTextBuffer m_buffer;

  WrenVM* m_vm;
  std::unordered_map<std::string, std::string> m_sources;
  std::mutex m_vmMutex;
  WrenHandle* m_appInstanceHandle;
  WrenHandle* m_renderMethodHandle;
};

}  // namespace MTerm
