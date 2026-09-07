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

# Stage 3: the pad map and the pure helpers, free of ROOT as well. The
# synthetic map of data/ is always compiled in; the real one only when it is
# there (TODO/07).
add_executable(readout_unit_tests test_readout_utils.cc)
target_link_libraries(readout_unit_tests PRIVATE readout_util GTest::gtest_main)
target_compile_definitions(readout_unit_tests PRIVATE
  SYNTHETIC_PADS_CSV_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data/pads_synthetic.csv")
if(EXISTS ${PADS_CSV})
  target_compile_definitions(readout_unit_tests PRIVATE
    PADS_CSV_PATH="${PADS_CSV}")
endif()

add_test(NAME readout_unit COMMAND readout_unit_tests)
set_tests_properties(readout_unit PROPERTIES LABELS unit)

# Stage 4: the AGET response and the pure helpers, free of ROOT as well.
add_executable(aget_unit_tests test_aget_utils.cc)
target_link_libraries(aget_unit_tests PRIVATE aget_util GTest::gtest_main)

add_test(NAME aget_unit COMMAND aget_unit_tests)
set_tests_properties(aget_unit PROPERTIES LABELS unit)

# scripts/make_channel_map.py builds the map of the analysis macros out of the
# pad map. Both files are geometry derived and not in git, so this runs only
# where they are: the script checks its 256 rows against the map that comes
# with the macros (TODO/07).
set(REAL_CHANNEL_MAP ${PROJECT_SOURCE_DIR}/external/tpcdaq-macros/channel_map.csv)
if(EXISTS ${PADS_CSV} AND EXISTS ${REAL_CHANNEL_MAP})
  add_test(NAME channel_map_unit
           COMMAND ${PYTHON3_EXECUTABLE}
                   ${PROJECT_SOURCE_DIR}/scripts/make_channel_map.py
                   ${PADS_CSV} channel_map.csv ${REAL_CHANNEL_MAP}
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(channel_map_unit PROPERTIES LABELS unit)
else()
  message(WARNING
    "geometry/pads.csv or external/tpcdaq-macros/channel_map.csv is missing: "
    "scripts/make_channel_map.py is not checked against the real map.")
endif()
