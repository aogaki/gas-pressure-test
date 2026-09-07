#pragma once

#include "G4String.hh"
#include "G4UserRunAction.hh"

class DetectorConstruction;
class PrimaryGeneratorAction;
struct TpcConfig;

// Opens the ROOT output, books the ntuples and applies the step limit that
// matches the current /tpc/hits setting.
class RunAction : public G4UserRunAction {
 public:
  RunAction(const TpcConfig& config, DetectorConstruction& detector,
            PrimaryGeneratorAction& generator);

  void BeginOfRunAction(const G4Run* run) override;
  void EndOfRunAction(const G4Run* run) override;

 private:
  // "5.5" for a mono-energetic source, otherwise the distribution name.
  G4String EnergyTag() const;
  void CreateNtuples();

  const TpcConfig& fConfig;
  DetectorConstruction& fDetector;
  PrimaryGeneratorAction& fGenerator;
  G4bool fNtuplesCreated = false;
};
