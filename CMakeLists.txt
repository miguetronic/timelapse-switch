cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)
# plugin-support (logging helper) uses the OBS log functions and their printf checks.
target_link_libraries(plugin-support PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/plugin-main.c)

# Power of Ten, rule 10 (docs/coding-rules.md): our own code builds with pedantic warnings and zero tolerance.
# OBS and Qt headers come from imported targets (system includes), so their warnings are not reported here.
set(
  _tsw_warnings
  -Wall
  -Wextra
  -Wpedantic
  -Wshadow
  -Wconversion
  -Wsign-conversion
  -Wformat=2
  -Wcast-qual
  -Wnull-dereference
  -Wimplicit-fallthrough
  -Wundef
  -Wunused-function
  -Werror
)
function(tsw_strict_warnings target)
  target_compile_options(
    ${target}
    PRIVATE
      "$<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:${_tsw_warnings};-Wstrict-prototypes;-Wmissing-prototypes>"
      "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:${_tsw_warnings}>"
      "$<$<COMPILE_LANG_AND_ID:C,MSVC>:/W4;/WX>"
      "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/W4;/WX>"
  )
endfunction()

# Core logic: pure C with no OBS dependency, shared by the plugin and the unit tests (docs/coding-rules.md, D1).
add_library(tsw-core STATIC src/core/gate.c src/core/timeline.c)
target_include_directories(tsw-core PUBLIC src)
set_target_properties(tsw-core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE tsw-core)

foreach(_tsw_target IN ITEMS ${CMAKE_PROJECT_NAME} plugin-support tsw-core)
  tsw_strict_warnings(${_tsw_target})
endforeach()

option(ENABLE_UNIT_TESTS "Build the unit tests (run them with ctest)" ON)
if(ENABLE_UNIT_TESTS)
  enable_testing()
  add_subdirectory(tests/unit)
endif()

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
