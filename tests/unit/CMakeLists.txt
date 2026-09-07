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

# Stage 2: the pure helpers only, so this binary stays free of Geant4,
# Garfield++ and ROOT.
add_executable(drift_unit_tests test_drift_utils.cc)
target_link_libraries(drift_unit_tests PRIVATE drift_util GTest::gtest_main)

add_test(NAME drift_unit COMMAND drift_unit_tests)
set_tests_properties(drift_unit PROPERTIES LABELS unit)
