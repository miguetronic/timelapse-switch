# The dock drawn without OBS (test-dock.cpp). Run with ctest; pictures land in <build>/ui-screenshots.
add_executable(test-dock test-dock.cpp)
target_link_libraries(test-dock PRIVATE tsw-dock Qt6::Widgets)
tsw_strict_warnings(test-dock)
add_test(NAME dock COMMAND test-dock "${CMAKE_SOURCE_DIR}/data/locale" "${CMAKE_BINARY_DIR}/ui-screenshots")
set_tests_properties(dock PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
