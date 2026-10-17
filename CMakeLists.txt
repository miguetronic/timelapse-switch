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

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE src/controller.c src/fault.c src/plugin-main.c src/recorder.c src/recorder-rt.c src/ui/dock-obs.cpp
)

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
add_library(
  tsw-core
  STATIC
  src/core/audio-ring.c
  src/core/color.c
  src/core/dock-view.c
  src/core/gate.c
  src/core/session.c
  src/core/timeline.c
)
target_include_directories(tsw-core PUBLIC src)
# Header-only parts of OBS (portable atomics); the core never links libobs, so tests run without OBS.
target_include_directories(tsw-core SYSTEM PUBLIC $<TARGET_PROPERTY:OBS::libobs,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(tsw-core PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(UNIX AND NOT APPLE)
  target_link_libraries(tsw-core PUBLIC m) # pow (color.c) lives in libm on Linux
endif()
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE tsw-core)

# The dock's widgets: Qt only, no OBS, so the UI test can draw them (tests/ui).
# Screen reader announcements need Qt 6.8; older Qt gets the fallback (rule P10-8: a file, not #if).
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.8)
  set(_tsw_announce src/ui/announce-qt68.cpp)
else()
  set(_tsw_announce src/ui/announce-legacy.cpp)
endif()
add_library(
  tsw-dock
  STATIC
  src/ui/dock.cpp
  src/ui/dock-glyph.cpp
  src/ui/dock-paint.cpp
  src/ui/dock-status.cpp
  src/ui/dock-theme.cpp
  src/ui/dock-widgets.cpp
  ${_tsw_announce}
)
target_link_libraries(tsw-dock PUBLIC tsw-core Qt6::Widgets)
set_target_properties(tsw-dock PROPERTIES AUTOMOC ON POSITION_INDEPENDENT_CODE ON)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE tsw-dock)

foreach(_tsw_target IN ITEMS ${CMAKE_PROJECT_NAME} plugin-support tsw-core tsw-dock)
  tsw_strict_warnings(${_tsw_target})
endforeach()

option(ENABLE_UNIT_TESTS "Build the unit tests (run them with ctest)" ON)
option(ENABLE_THEME_PROBE "Build the test-only OBS module used by tests/ui/obs-themes.sh" ON)
if(ENABLE_UNIT_TESTS)
  enable_testing()
  add_subdirectory(tests/unit)
  add_subdirectory(tests/ui)
endif()

option(ENABLE_INTEGRATION_TESTS "Build the test-only OBS module used by tests/integration/run.sh" ON)
if(ENABLE_INTEGRATION_TESTS)
  add_subdirectory(tests/integration)
endif()

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
