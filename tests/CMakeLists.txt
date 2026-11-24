add_executable(lex-core-tests
  test_main.cpp
  NullPlatform.h
  NullPlatform.cpp
)

target_include_directories(lex-core-tests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(lex-core-tests PRIVATE lex-core lex-modules-objects)

add_test(NAME lex-core-tests COMMAND lex-core-tests)
