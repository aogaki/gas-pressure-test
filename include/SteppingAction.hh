#pragma once

#include "G4UserSteppingAction.hh"

class DetectorConstruction;
class EventAction;
struct TpcConfig;

// Accumulates the gas energy deposit and the primary track length, and writes
// the hits ntuple when /tpc/hits is on.
class SteppingAction : public G4UserSteppingAction {
 public:
  SteppingAction(const TpcConfig& config, const DetectorConstruction& detector,
                 EventAction& eventAction);

  void UserSteppingAction(const G4Step* step) override;

 private:
  const TpcConfig& fConfig;
  const DetectorConstruction& fDetector;
  EventAction& fEventAction;
};
