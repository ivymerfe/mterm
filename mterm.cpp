#include "MTerm.h"

#include <algorithm>
#include <cstdio>

#include "Utils.h"
#include "defaults.h"

extern "C" __declspec(dllimport) void __stdcall OutputDebugStringA(
    const char* lpOutputString);

namespace MTerm {

static void WrenPrint(WrenVM* vm, const char* text) {
  OutputDebugStringA(text);
}

static void WrenError(WrenVM* vm,
                      WrenErrorType type,
                      const char* module,
                      int line,
                      const char* message) {
  OutputDebugStringA("Error in module ");
  OutputDebugStringA(module);
  OutputDebugStringA(" at line ");
  char buf[64];
  sprintf(buf, "%d", line);
  OutputDebugStringA(buf);
  OutputDebugStringA(" : ");
  OutputDebugStringA(message);
  OutputDebugStringA("\n");
}

MTerm::MTerm() {}

void MTerm::Init(void* window) {
  WrenConfiguration config;
  wrenInitConfiguration(&config);
  config.writeFn = &WrenPrint;
  config.errorFn = &WrenError;
  config.loadModuleFn = MTerm::LoadModule;
  config.bindForeignClassFn = MTerm::BindForeignClass;
  config.bindForeignMethodFn = MTerm::BindForeignMethod;
  config.userData = this;
  m_vm = wrenNewVM(
      &config);  // TODO - takes around 13ms. Maybe do it in separate thread

  WrenInterpretResult result = wrenInterpret(m_vm, NULL, "import \"main\"");
  if (result != WREN_RESULT_SUCCESS) {
    throw std::exception("Failed to load core module.");
  }
  wrenEnsureSlots(m_vm, 1);
  wrenGetVariable(m_vm, "main", "App", 0);
  WrenHandle* app_class_handle = wrenGetSlotHandle(m_vm, 0);

  // Call App.new() to create an instance
  WrenHandle* constructorHandle = wrenMakeCallHandle(m_vm, "new()");
  wrenEnsureSlots(m_vm, 1);
  wrenSetSlotHandle(m_vm, 0, app_class_handle);
  result = wrenCall(m_vm, constructorHandle);
  wrenReleaseHandle(m_vm, constructorHandle);

  if (result != WREN_RESULT_SUCCESS) {
    throw std::exception("Failed to instantiate App class.");
  }

  // Store instance handle
  m_appInstanceHandle = wrenGetSlotHandle(m_vm, 0);

  // Prepare method call handle (instance method, not class method!)
  m_renderMethodHandle = wrenMakeCallHandle(m_vm, "render()");

  m_renderer.Init(window, FONT_NAME, [this]() { this->Render(); });

  m_width = m_renderer.GetWidth();
  m_height = m_renderer.GetHeight();

  m_isInitialized = true;
}

void MTerm::Destroy() {
  m_renderer.Destroy();
  wrenFreeVM(m_vm);
}

bool MTerm::IsInitialized() {
  return m_isInitialized;
}

WrenLoadModuleResult MTerm::LoadModule(WrenVM* vm, const char* name) {
  std::string path = "modules/" + std::string(name) + ".wren";
  MTerm* this_ptr = (MTerm*)wrenGetUserData(vm);
  std::unordered_map<std::string, std::string>& source_map =
      this_ptr->m_sources;
  WrenLoadModuleResult result = {0};
  if (source_map.contains(path)) {
    result.source = source_map[path].c_str();
  } else {
    auto file_read_result = Utils::GetFileContent(path.c_str());

    if (file_read_result.has_value()) {
      source_map[path] = file_read_result.value();
      result.source = source_map[path].c_str();
    }
  }
  return result;
}

WrenForeignClassMethods MTerm::BindForeignClass(WrenVM* vm,
                                                const char* module,
                                                const char* className) {
  WrenForeignClassMethods methods = {};

  if (std::string(module) == "renderer" &&
      std::string(className) == "Renderer") {
    methods.allocate = [](WrenVM* vm) {
      MTerm* term = static_cast<MTerm*>(wrenGetUserData(vm));
      // Store pointer to renderer in foreign object slot
      Renderer** rendererPtr =
          (Renderer**)wrenSetSlotNewForeign(vm, 0, 0, sizeof(Renderer*));
      *rendererPtr = &term->m_renderer;
    };

    methods.finalize = [](void* data) {
      // no-op: renderer is owned by MTerm
    };
  }

  return methods;
}

WrenForeignMethodFn MTerm::BindForeignMethod(WrenVM* vm,
                                             const char* module,
                                             const char* className,
                                             bool isStatic,
                                             const char* signature) {
  if (std::string(module) == "renderer" &&
      std::string(className) == "Renderer") {
    if (std::string(signature) == "clear(_)") {
      return [](WrenVM* vm) {
        Renderer* r = *(Renderer**)wrenGetSlotForeign(vm, 0);
        int color = (int)wrenGetSlotDouble(vm, 1);
        r->Clear(color);
      };
    }

    if (std::string(signature) == "line(_,_,_,_,_,_,_)") {
      return [](WrenVM* vm) {
        Renderer* r = *(Renderer**)wrenGetSlotForeign(vm, 0);
        float x1 = (float)wrenGetSlotDouble(vm, 1);
        float y1 = (float)wrenGetSlotDouble(vm, 2);
        float x2 = (float)wrenGetSlotDouble(vm, 3);
        float y2 = (float)wrenGetSlotDouble(vm, 4);
        float thickness = (float)wrenGetSlotDouble(vm, 5);
        int color = (int)wrenGetSlotDouble(vm, 6);
        float opacity = (float)wrenGetSlotDouble(vm, 7);
        r->Line(x1, y1, x2, y2, thickness, color, opacity);
      };
    }

    if (std::string(signature) == "rect(_,_,_,_,_,_)") {
      return [](WrenVM* vm) {
        Renderer* r = *(Renderer**)wrenGetSlotForeign(vm, 0);
        float l = (float)wrenGetSlotDouble(vm, 1);
        float t = (float)wrenGetSlotDouble(vm, 2);
        float rgt = (float)wrenGetSlotDouble(vm, 3);
        float btm = (float)wrenGetSlotDouble(vm, 4);
        int color = (int)wrenGetSlotDouble(vm, 5);
        float opacity = (float)wrenGetSlotDouble(vm, 6);
        r->Rect(l, t, rgt, btm, color, opacity);
      };
    }

    if (std::string(signature) == "outline(_,_,_,_,_,_,_)") {
      return [](WrenVM* vm) {
        Renderer* r = *(Renderer**)wrenGetSlotForeign(vm, 0);
        float l = (float)wrenGetSlotDouble(vm, 1);
        float t = (float)wrenGetSlotDouble(vm, 2);
        float rgt = (float)wrenGetSlotDouble(vm, 3);
        float btm = (float)wrenGetSlotDouble(vm, 4);
        float thickness = (float)wrenGetSlotDouble(vm, 5);
        int color = (int)wrenGetSlotDouble(vm, 6);
        float opacity = (float)wrenGetSlotDouble(vm, 7);
        r->Outline(l, t, rgt, btm, thickness, color, opacity);
      };
    }

    if (std::string(signature) == "text(_,_,_,_,_,_,_,_)") {
      return [](WrenVM* vm) {
        Renderer* r = *(Renderer**)wrenGetSlotForeign(vm, 0);

        const char* utf8_text = wrenGetSlotString(vm, 1);
        std::vector<char32_t> u32_text =
            Utils::Utf8ToUtf32(utf8_text, strlen(utf8_text));
        float fontSize = (float)wrenGetSlotDouble(vm, 2);
        float x = (float)wrenGetSlotDouble(vm, 3);
        float y = (float)wrenGetSlotDouble(vm, 4);
        int color = (int)wrenGetSlotDouble(vm, 5);
        int underlineColor = (int)wrenGetSlotDouble(vm, 6);
        int backgroundColor = (int)wrenGetSlotDouble(vm, 7);
        float opacity = (float)wrenGetSlotDouble(vm, 8);

        r->Text(u32_text.data(), (int)u32_text.size(), fontSize, x, y, color,
                underlineColor, backgroundColor, opacity);
      };
    }
  }
  if (std::strcmp(module, "main") == 0 && std::strcmp(className, "App") == 0 &&
      isStatic) {
    MTerm* self = static_cast<MTerm*>(wrenGetUserData(vm));

    if (std::strcmp(signature, "getWidth()") == 0) {
      return [](WrenVM* vm) {
        MTerm* self = static_cast<MTerm*>(wrenGetUserData(vm));
        wrenSetSlotDouble(vm, 0, self->m_width);
      };
    }

    if (std::strcmp(signature, "getHeight()") == 0) {
      return [](WrenVM* vm) {
        MTerm* self = static_cast<MTerm*>(wrenGetUserData(vm));
        wrenSetSlotDouble(vm, 0, self->m_height);
      };
    }
  }
  return nullptr;  // Not found
}

void MTerm::Redraw() {
  m_renderer.Redraw();
}

void MTerm::Resize(unsigned int width, unsigned int height) {
  m_width = width;
  m_height = height;
  m_renderer.Resize(width, height);
}

void MTerm::Render() {
  wrenEnsureSlots(m_vm, 1);
  wrenSetSlotHandle(m_vm, 0, m_appInstanceHandle);
  WrenInterpretResult result = wrenCall(m_vm, m_renderMethodHandle);
  if (result != WREN_RESULT_SUCCESS) {
    OutputDebugStringA("Failed to invoke render()\n");
  }
}

void MTerm::KeyDown(int key_code) {}

void MTerm::KeyUp(int key_code) {}

void MTerm::Input(char32_t key) {}

void MTerm::MouseMove(int x, int y) {}

void MTerm::MouseDown(int x, int y, int button) {}

void MTerm::MouseUp(int x, int y, int button) {}

void MTerm::Scroll(int x, int y, int delta) {
  // if (GetAsyncKeyState(VK_LCONTROL) & 0x8000) {
  //   float new_font_size = m_fontSize + delta;
  //   if (2 < new_font_size && new_font_size < 200) {
  //     m_fontSize = new_font_size;
  //     Redraw();
  //   }
  // } else {
  //   if (m_terminal != nullptr) {
  //     m_terminal->Scroll(x, y, delta);
  //   }
  // }
}

}  // namespace MTerm
