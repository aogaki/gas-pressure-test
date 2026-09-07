#pragma once

#include "G4String.hh"
#include "G4Types.hh"

// Run configuration driven by the /tpc/ commands (see TpcMessenger).
struct TpcConfig {
  G4String gas = "Ar";
  G4double pressureMbar = 1013.25;
  G4bool hits = false;
};
