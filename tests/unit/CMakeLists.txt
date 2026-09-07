find_package(GTest CONFIG REQUIRED)

add_executable(unit_tests
  test_gas_properties.cc
  test_gas_material.cc
  test_detector.cc
  test_primary_generator.cc
)
target_link_libraries(unit_tests PRIVATE gpt_core GTest::gtest_main)

add_test(NAME unit COMMAND unit_tests)
set_tests_properties(unit PROPERTIES LABELS unit)
