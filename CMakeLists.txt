cmake_minimum_required(VERSION 3.20)
project(gas-pressure-test LANGUAGES CXX)

# Optimised by default: the runs are long and the speed is a deliverable.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

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

enable_testing()
add_subdirectory(tests/unit)
add_subdirectory(tests/acceptance)
