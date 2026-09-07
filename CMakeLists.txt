cmake_minimum_required(VERSION 3.20)
project(gas-pressure-test LANGUAGES CXX)

# Optimised by default: the runs are long and the speed is a deliverable.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# For clangd in the editor (see .clangd).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Geant4 REQUIRED)

# Everything except main() lives in a library so that the unit tests can link it.
add_library(gpt_core STATIC
  src/GasProperties.cc
  src/GasMaterial.cc
  src/DetectorConstruction.cc
  src/TpcMessenger.cc
  src/PhysicsList.cc
  src/PrimaryGeneratorAction.cc
  src/RunAction.cc
  src/EventAction.cc
  src/SteppingAction.cc
  src/ActionInitialization.cc
)
target_include_directories(gpt_core PUBLIC
  ${PROJECT_SOURCE_DIR}/include
  ${Geant4_INCLUDE_DIRS}
)
target_link_libraries(gpt_core PUBLIC ${Geant4_LIBRARIES})

add_executable(gas-pressure-test src/main.cc)
target_link_libraries(gas-pressure-test PRIVATE gpt_core)

# --- Stage 2: drift-electrons (Garfield++ and ROOT, no Geant4) --------------
list(APPEND CMAKE_PREFIX_PATH /opt/Garfield /opt/ROOT)
find_package(Garfield REQUIRED)
find_package(ROOT REQUIRED COMPONENTS RIO Tree)

# The pure helpers, so that the unit tests need neither Garfield++ nor ROOT.
# GasProperties.cc has no Geant4 dependency either, and both stages parse the
# same gas specification, so it is compiled into this library as well.
add_library(drift_util STATIC src/DriftUtils.cc src/GasProperties.cc)
target_include_directories(drift_util PUBLIC ${PROJECT_SOURCE_DIR}/include)

add_library(drift_core STATIC src/Drifter.cc)
target_link_libraries(drift_core PUBLIC drift_util Garfield::Garfield)

add_executable(drift-electrons src/drift_main.cc)
target_link_libraries(drift-electrons PRIVATE drift_core ROOT::RIO ROOT::Tree)

# --- Stage 3: channel-response (ROOT only, no Geant4 and no Garfield++) -----
# The pad map and the pure helpers, so that the unit tests need no ROOT.
add_library(readout_util STATIC src/ChannelUtils.cc)
target_include_directories(readout_util PUBLIC ${PROJECT_SOURCE_DIR}/include)

add_executable(channel-response src/channel_main.cc)
target_link_libraries(channel-response PRIVATE readout_util ROOT::RIO ROOT::Tree)

enable_testing()
add_subdirectory(tests/unit)
add_subdirectory(tests/acceptance)
