#pragma once

#include "G4VUserActionInitialization.hh"

class DetectorConstruction;
struct TpcConfig;

class ActionInitialization : public G4VUserActionInitialization {
 public:
  ActionInitialization(const TpcConfig& config, DetectorConstruction& detector);

  void Build() const override;

 private:
  const TpcConfig& fConfig;
  DetectorConstruction& fDetector;
};
