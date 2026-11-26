cmake_minimum_required(VERSION 3.22)
project(lex_android CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  URL "https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.tar.gz"
  URL_HASH SHA256=0d8ef5af7f9794e3263480193c491549b2ba6cc74bb018906202ada498a79406
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)
FetchContent_Declare(
  imgui
  URL "https://github.com/ocornut/imgui/archive/refs/tags/v1.91.9.tar.gz"
  URL_HASH SHA256=3872a5f90df78fced023c1945f4466b654fd74573370b77b17742149763a7a7c
)
FetchContent_MakeAvailable(imgui)

# liblex.so: GameActivity carga este puente por android.app.lib_name (ver
# forwarder.cpp y el manifiesto).
add_library(lex SHARED
  forwarder.cpp
  got.cpp
  overlay.cpp
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(lex PRIVATE ${imgui_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(lex PRIVATE IMGUI_IMPL_OPENGL_ES3)
target_link_libraries(lex PRIVATE nlohmann_json::nlohmann_json EGL GLESv3 android log dl)
# Solo ANativeActivity_onCreate hacia fuera; libc++ estatica para no chocar
# con la del juego.
target_link_options(lex PRIVATE -Wl,--exclude-libs,ALL -static-libstdc++)
set_target_properties(lex PROPERTIES CXX_VISIBILITY_PRESET hidden)
