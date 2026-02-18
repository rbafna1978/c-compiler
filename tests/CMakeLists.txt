add_executable(unit_tests
  ${CMAKE_SOURCE_DIR}/src/lexer/lexer.cpp
  ${CMAKE_SOURCE_DIR}/src/parser/parser.cpp
  ${CMAKE_SOURCE_DIR}/src/ast/ast.cpp
  ${CMAKE_BINARY_DIR}/generated/parser.cpp
  ${CMAKE_BINARY_DIR}/generated/lexer.cpp
  unit/test_lexer.cpp
  unit/test_parser.cpp
  unit/test_sema.cpp
  unit/test_codegen.cpp
)

add_dependencies(unit_tests generate_frontend)

set_source_files_properties(
  ${CMAKE_BINARY_DIR}/generated/lexer.cpp
  PROPERTIES COMPILE_OPTIONS "-Wno-error"
)

set_source_files_properties(
  ${CMAKE_BINARY_DIR}/generated/parser.cpp
  PROPERTIES COMPILE_OPTIONS "-Wno-error"
)

target_link_libraries(unit_tests PRIVATE GTest::gtest_main)

target_include_directories(unit_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_BINARY_DIR}/generated
)

include(GoogleTest)
gtest_discover_tests(unit_tests)
