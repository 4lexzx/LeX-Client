cmake_minimum_required(VERSION 3.25)

project(LeXClient
  VERSION 0.3.0
  DESCRIPTION "LeX-Client: nucleo portable C++20 + capa Windows"
  LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "LeX-Client requiere compilacion x64 (64 bits).")
endif()

add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)

if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8 /MP)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
  add_compile_options(-Wall -Wextra -Wpedantic "-fmacro-prefix-map=${CMAKE_SOURCE_DIR}/=")
endif()

# ---------------------------------------------------------------------------
# Dependencia: nlohmann/json 3.11.3 (version fijada + hash).
# Alternativa con submódulo:
#   git submodule add https://github.com/nlohmann/json.git third_party/json
#   add_subdirectory(third_party/json)
# ---------------------------------------------------------------------------
include(FetchContent)
set(JSON_BuildTests OFF CACHE INTERNAL "Desactivar tests de nlohmann/json" FORCE)
FetchContent_Declare(
  nlohmann_json
  URL "https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.tar.gz"
  URL_HASH SHA256=0d8ef5af7f9794e3263480193c491549b2ba6cc74bb018906202ada498a79406
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# ---------------------------------------------------------------------------
# Dependencia: Dear ImGui 1.91.9. El repo no trae CMakeLists, asi que se
# puebla con FetchContent_Populate y se define nuestro propio target.
# ---------------------------------------------------------------------------
FetchContent_Declare(
  imgui
  URL "https://github.com/ocornut/imgui/archive/refs/tags/v1.91.9.tar.gz"
  URL_HASH SHA256=3872a5f90df78fced023c1945f4466b654fd74573370b77b17742149763a7a7c
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
if(POLICY CMP0169)
  cmake_policy(SET CMP0169 OLD)
endif()
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
  FetchContent_Populate(imgui)
endif()

add_library(lex-imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
)
target_include_directories(lex-imgui PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)

enable_language(RC)

# Iconos propios: los PNG/RGBA se generan con scripts/make-icons.mjs y se
# empaquetan como RCDATA dentro de la DLL.
set(LEX_ICON_DIR "${CMAKE_SOURCE_DIR}/assets/icons")
file(TO_CMAKE_PATH "${LEX_ICON_DIR}" LEX_ICON_DIR)
configure_file(
  "${CMAKE_SOURCE_DIR}/platform/windows/resources/icons.rc.in"
  "${CMAKE_CURRENT_BINARY_DIR}/icons.rc"
  @ONLY
)

add_library(lex-minhook STATIC
  third_party/minhook/src/buffer.c third_party/minhook/src/hook.c
  third_party/minhook/src/trampoline.c third_party/minhook/src/hde/hde64.c)
target_include_directories(lex-minhook PUBLIC third_party/minhook/include)
if(MINGW)
  # La DLL no puede depender de libstdc++/libwinpthread dentro del juego.
  add_link_options(-static -s)
  target_compile_options(lex-minhook PRIVATE -masm=intel -w)
endif()

enable_testing()

add_subdirectory(core)
add_subdirectory(platform/windows)
add_subdirectory(tests)

# ---------------------------------------------------------------------------
# Salida de la Fase 1: LeXClient.dll (aun sin hooks; expone el ciclo de vida).
# ---------------------------------------------------------------------------
add_library(LeXClient SHARED
  platform/windows/dllmain.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/icons.rc
)
target_include_directories(LeXClient PRIVATE
  "${CMAKE_SOURCE_DIR}/platform/windows/resources"
)
target_compile_definitions(LeXClient PRIVATE LEX_BUILD_DLL)
target_link_libraries(LeXClient PRIVATE lex-core lex-platform-windows)
set_target_properties(LeXClient PROPERTIES OUTPUT_NAME "LeXClient" PREFIX "")

add_executable(lex-injector tools/injector.cpp)
if(MINGW)
  target_link_options(lex-injector PRIVATE -municode)
endif()

# Banco visual: aloja LeXClient.dll en una ventana D3D11/D3D12 sin el juego.
add_executable(lex-preview tools/preview.cpp)
target_link_libraries(lex-preview PRIVATE d3d11 d3d12 dxgi dxguid user32 gdi32)
add_dependencies(lex-preview LeXClient)

# Exporta el catalogo de modulos que usa el launcher.
add_executable(lex-catalog tools/catalog.cpp)
target_link_libraries(lex-catalog PRIVATE lex-core lex-platform-windows)
