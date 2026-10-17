# Tests of the developer tools in tools/. Run them with: ctest --test-dir build_x86_64
if(UNIX AND NOT APPLE)
  add_test(NAME dev-install COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/test-dev-install.sh")
endif()
