#pragma once

#include "G4Material.hh"
#include "G4String.hh"

// Builds (or returns the already built) gas material for the given gas name
// and pressure [mbar]. The material keeps the NIST material as its base
// material so that the alpha stopping power tables are looked up correctly.
G4Material* BuildGasMaterial(const G4String& gas, G4double pressureMbar);
