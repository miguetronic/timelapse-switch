# Unit tests of the core logic (docs/coding-rules.md, D1). Run them with: ctest --test-dir build_x86_64
add_executable(test-timeline test-timeline.c test-main.c)
target_link_libraries(test-timeline PRIVATE tsw-core)
tsw_strict_warnings(test-timeline)
add_test(NAME timeline COMMAND test-timeline)

add_executable(test-gate test-gate.c test-main.c)
target_link_libraries(test-gate PRIVATE tsw-core)
tsw_strict_warnings(test-gate)
add_test(NAME gate COMMAND test-gate)

add_executable(test-sync test-sync.c test-main.c)
target_link_libraries(test-sync PRIVATE tsw-core)
if(UNIX AND NOT APPLE)
  target_link_libraries(test-sync PRIVATE m) # fabs and fmax live in libm on Linux
endif()
tsw_strict_warnings(test-sync)
add_test(NAME sync COMMAND test-sync)

add_executable(test-audio-ring test-audio-ring.c test-main.c)
target_link_libraries(test-audio-ring PRIVATE tsw-core)
tsw_strict_warnings(test-audio-ring)
add_test(NAME audio-ring COMMAND test-audio-ring)

add_executable(test-session test-session.c test-main.c)
target_link_libraries(test-session PRIVATE tsw-core)
tsw_strict_warnings(test-session)
add_test(NAME session COMMAND test-session)

add_executable(test-dock-view test-dock-view.c test-main.c)
target_link_libraries(test-dock-view PRIVATE tsw-core)
tsw_strict_warnings(test-dock-view)
add_test(NAME dock-view COMMAND test-dock-view)

add_executable(test-color test-color.c test-main.c)
target_link_libraries(test-color PRIVATE tsw-core)
tsw_strict_warnings(test-color)
add_test(NAME color COMMAND test-color)

add_executable(test-health test-health.c test-main.c)
target_link_libraries(test-health PRIVATE tsw-core)
tsw_strict_warnings(test-health)
add_test(NAME health COMMAND test-health)
