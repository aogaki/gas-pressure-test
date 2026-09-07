#include "DetectorConstruction.hh"

#include <cfloat>

#include "G4Box.hh"
#include "G4LogicalVolume.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"
#include "G4UserLimits.hh"
#include "GasMaterial.hh"
#include "TpcConfig.hh"

namespace {

// Step limit inside the gas when hits are recorded.
constexpr G4double kHitsMaxStep = 1. * mm;

}  // namespace

DetectorConstruction::DetectorConstruction(const TpcConfig& config)
    : fConfig(config) {}

G4VPhysicalVolume* DetectorConstruction::Construct() {
  G4Material* vacuum =
      G4NistManager::Instance()->FindOrBuildMaterial("G4_Galactic");
  G4Material* gas = BuildGasMaterial(fConfig.gas, fConfig.pressureMbar);

  // World: 1 m cube.
  auto* worldSolid = new G4Box("World", 500. * mm, 500. * mm, 500. * mm);
  auto* worldLogical = new G4LogicalVolume(worldSolid, vacuum, "World");
  auto* worldPhysical = new G4PVPlacement(nullptr, G4ThreeVector(), worldLogical,
                                          "World", nullptr, false, 0, true);

  // Gas volume: 200 x 200 x 500 mm, centred on the origin.
  auto* gasSolid = new G4Box("Gas", 100. * mm, 100. * mm, 250. * mm);
  fGasLogical = new G4LogicalVolume(gasSolid, gas, "Gas");
  new G4PVPlacement(nullptr, G4ThreeVector(), fGasLogical, "Gas", worldLogical,
                    false, 0, true);

  fGasLimits = new G4UserLimits();
  fGasLogical->SetUserLimits(fGasLimits);
  ApplyStepLimit();

  return worldPhysical;
}

void DetectorConstruction::ApplyStepLimit() {
  if (fGasLimits == nullptr) return;
  fGasLimits->SetMaxAllowedStep(fConfig.hits ? kHitsMaxStep : DBL_MAX);
}
