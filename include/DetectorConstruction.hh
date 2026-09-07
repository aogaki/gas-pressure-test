#pragma once

#include "G4VUserDetectorConstruction.hh"

class G4LogicalVolume;
class G4UserLimits;
struct TpcConfig;

// World (1 m cube of vacuum) containing a 200 x 200 x 500 mm gas box centred
// on the origin. The gas is defined by the TpcConfig given from outside.
class DetectorConstruction : public G4VUserDetectorConstruction {
 public:
  explicit DetectorConstruction(const TpcConfig& config);

  G4VPhysicalVolume* Construct() override;

  G4LogicalVolume* GasLogicalVolume() const { return fGasLogical; }

  // Sets the gas step limit according to the current /tpc/hits setting.
  void ApplyStepLimit();

 private:
  const TpcConfig& fConfig;
  G4LogicalVolume* fGasLogical = nullptr;
  G4UserLimits* fGasLimits = nullptr;
};
