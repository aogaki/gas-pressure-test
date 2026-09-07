#pragma once

#include "G4VModularPhysicsList.hh"

// EM only: option4 for the best low energy accuracy plus the step limiter
// that /tpc/hits uses. No hadronic physics and no decay, see TODO/01.
class PhysicsList : public G4VModularPhysicsList {
 public:
  PhysicsList();
};
