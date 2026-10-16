# The dock drawn without OBS (test-dock.cpp). Run with ctest; pictures land in <build>/ui-screenshots.
# Given the dumps of tests/ui/obs-themes.sh, it also draws the dock in every OBS theme (README.md here).
add_executable(test-dock test-dock.cpp obs-theme.cpp contact-sheet.cpp)
target_link_libraries(test-dock PRIVATE tsw-dock Qt6::Widgets)
set_target_properties(test-dock PROPERTIES AUTOMOC ON)
tsw_strict_warnings(test-dock)
add_test(NAME dock COMMAND test-dock "${CMAKE_SOURCE_DIR}/data/locale" "${CMAKE_BINARY_DIR}/ui-screenshots")
set_tests_properties(dock PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# The OBS module that obs-themes.sh installs in a sandbox OBS to write each theme down (README.md
# here). Linux only, like the script. Never shipped.
if(ENABLE_THEME_PROBE AND OS_LINUX)
  add_library(tsw-theme-probe MODULE theme-probe.cpp)
  target_link_libraries(tsw-theme-probe PRIVATE OBS::libobs OBS::obs-frontend-api Qt6::Widgets)
  set_target_properties(
    tsw-theme-probe
    PROPERTIES PREFIX "" LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/ui-theme-probe"
  )
  tsw_strict_warnings(tsw-theme-probe)
endif()
