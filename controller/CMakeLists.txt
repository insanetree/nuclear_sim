add_library(controller
    src/decision_making.cpp
)

target_include_directories(controller PUBLIC include)
target_link_libraries(controller PUBLIC logging)
set_project_warnings(controller)

add_executable(controller_exe
    src/main.cpp
)
target_link_libraries(controller_exe PRIVATE controller)
set_project_warnings(controller_exe)
set_target_properties(controller_exe PROPERTIES OUTPUT_NAME controller)

# add_executable(test_control_rods test/test_control_rods.cpp)
# target_link_libraries(test_control_rods PRIVATE reactor GTest::gtest_main)
# set_project_warnings(test_control_rods)
# add_test(NAME test_control_rods COMMAND test_control_rods)
# 
# add_executable(test_core test/test_core.cpp)
# target_link_libraries(test_core PRIVATE reactor GTest::gtest_main)
# set_project_warnings(test_core)
# add_test(NAME test_core COMMAND test_core)
# 
# add_executable(test_coolant_loop test/test_coolant_loop.cpp)
# target_link_libraries(test_coolant_loop PRIVATE reactor GTest::gtest_main)
# set_project_warnings(test_coolant_loop)
# add_test(NAME test_coolant_loop COMMAND test_coolant_loop)
# 
# add_executable(test_turbine test/test_turbine.cpp)
# target_link_libraries(test_turbine PRIVATE reactor GTest::gtest_main)
# set_project_warnings(test_turbine)
# add_test(NAME test_turbine COMMAND test_turbine)
# 
# add_executable(test_coordinator test/test_coordinator.cpp)
# target_link_libraries(test_coordinator PRIVATE reactor GTest::gtest_main)
# set_project_warnings(test_coordinator)
# add_test(NAME test_coordinator COMMAND test_coordinator)
# set_tests_properties(test_coordinator PROPERTIES
#     ENVIRONMENT "TSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/tsan_suppressions.txt"
# )
