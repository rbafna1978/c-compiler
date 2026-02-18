cmake_minimum_required(VERSION 3.20)
project(compiler LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Werror)

if(NOT DEFINED LLVM_DIR OR NOT LLVM_DIR OR LLVM_DIR STREQUAL "LLVM_DIR-NOTFOUND")
  find_program(LLVM_CONFIG_EXECUTABLE NAMES llvm-config llvm-config-17)
  if(LLVM_CONFIG_EXECUTABLE)
    execute_process(
      COMMAND ${LLVM_CONFIG_EXECUTABLE} --cmakedir
      OUTPUT_VARIABLE LLVM_CMAKE_DIR
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(EXISTS "${LLVM_CMAKE_DIR}/LLVMConfig.cmake")
      set(LLVM_DIR "${LLVM_CMAKE_DIR}" CACHE PATH "LLVM CMake directory")
    endif()
  endif()

  if(NOT DEFINED LLVM_DIR OR NOT LLVM_DIR OR LLVM_DIR STREQUAL "LLVM_DIR-NOTFOUND")
    foreach(candidate
      "/opt/homebrew/opt/llvm@17/lib/cmake/llvm"
      "/usr/local/opt/llvm@17/lib/cmake/llvm"
      "/usr/lib/llvm-17/lib/cmake/llvm"
      "/usr/local/lib/cmake/llvm"
    )
      if(EXISTS "${candidate}/LLVMConfig.cmake")
        set(LLVM_DIR "${candidate}" CACHE PATH "LLVM CMake directory")
        break()
      endif()
    endforeach()
  endif()
endif()

find_package(LLVM 17 REQUIRED CONFIG)
find_package(GTest REQUIRED)
find_program(FLEX_EXECUTABLE NAMES flex REQUIRED)
find_program(
  BISON_EXECUTABLE
  NAMES bison
  HINTS /opt/homebrew/opt/bison/bin /usr/local/opt/bison/bin
  REQUIRED
)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

set(GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

set(LEXER_INPUT ${CMAKE_SOURCE_DIR}/src/lexer/lexer.l)
set(PARSER_INPUT ${CMAKE_SOURCE_DIR}/src/parser/parser.y)
set(LEXER_OUTPUT ${GENERATED_DIR}/lexer.cpp)
set(PARSER_OUTPUT ${GENERATED_DIR}/parser.cpp)
set(PARSER_HEADER ${GENERATED_DIR}/parser.hpp)

add_custom_command(
  OUTPUT ${LEXER_OUTPUT}
  COMMAND ${FLEX_EXECUTABLE} -o ${LEXER_OUTPUT} ${LEXER_INPUT}
  DEPENDS ${LEXER_INPUT}
  COMMENT "Generating lexer with Flex"
  VERBATIM
)

add_custom_command(
  OUTPUT ${PARSER_OUTPUT} ${PARSER_HEADER}
  COMMAND ${BISON_EXECUTABLE} --defines=${PARSER_HEADER} --output=${PARSER_OUTPUT} ${PARSER_INPUT}
  DEPENDS ${PARSER_INPUT}
  COMMENT "Generating parser with Bison"
  VERBATIM
)

add_custom_target(generate_frontend DEPENDS ${LEXER_OUTPUT} ${PARSER_OUTPUT} ${PARSER_HEADER})

set_source_files_properties(${LEXER_OUTPUT} PROPERTIES COMPILE_OPTIONS "-Wno-error")
set_source_files_properties(${PARSER_OUTPUT} PROPERTIES COMPILE_OPTIONS "-Wno-error")

llvm_map_components_to_libnames(LLVM_LIBS
  Core
  Support
  IRReader
  Passes
)

add_executable(compiler
  src/main.cpp
  src/lexer/lexer.cpp
  src/parser/parser.cpp
  src/ast/ast.cpp
  src/sema/sema.cpp
  src/sema/symbol_table.cpp
  src/codegen/codegen.cpp
  src/optimizer/optimizer.cpp
  ${LEXER_OUTPUT}
  ${PARSER_OUTPUT}
)

add_dependencies(compiler generate_frontend)

target_include_directories(compiler PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/lexer
  ${CMAKE_SOURCE_DIR}/src/parser
  ${CMAKE_SOURCE_DIR}/src/ast
  ${CMAKE_SOURCE_DIR}/src/sema
  ${CMAKE_SOURCE_DIR}/src/codegen
  ${CMAKE_SOURCE_DIR}/src/optimizer
  ${GENERATED_DIR}
)

target_link_libraries(compiler PRIVATE ${LLVM_LIBS})

enable_testing()
add_subdirectory(tests)
