# The OBS module that the integration test installs next to the plugin (README.md here). Never shipped.
add_library(tsw-sync-probe MODULE sync-probe.c)
target_link_libraries(tsw-sync-probe PRIVATE OBS::libobs)
set_target_properties(tsw-sync-probe PROPERTIES PREFIX "" LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/integration")
tsw_strict_warnings(tsw-sync-probe)
