# Unit tests of the core logic (docs/coding-rules.md, D1). Run them with: ctest --test-dir build_x86_64
add_executable(test-timeline test-timeline.c test-main.c)
target_link_libraries(test-timeline PRIVATE tsw-core)
tsw_strict_warnings(test-timeline)
add_test(NAME timeline COMMAND test-timeline)
