cmake_minimum_required (VERSION 3.12)

# Включение горячей перезагрузки для компиляторов MSVC, если поддерживается.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("mterm")

add_compile_definitions(UNICODE _UNICODE)

add_executable (mterm WIN32 "main.cpp" "resource.rc"    "Utils.h" "Utils.cpp" "PseudoConsole.h" "PseudoConsole.cpp" "Window.h" "Window.cpp" "ColoredTextBuffer.h" "ColoredTextBuffer.cpp")

target_link_libraries (mterm PRIVATE dxguid.lib d2d1.lib dwrite.lib)

set_property(TARGET mterm PROPERTY CXX_STANDARD 20)
