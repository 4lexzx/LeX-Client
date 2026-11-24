# OBJECT library: los modulos se auto-registran en la inicializacion estatica.
# Al ser OBJECT (no STATIC), sus .cpp se enlazan siempre en cada consumidor.
add_library(lex-modules-objects OBJECT
  test/TestModule.h
  test/TestModule.cpp
)

target_include_directories(lex-modules-objects PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(lex-modules-objects PUBLIC lex-core)
