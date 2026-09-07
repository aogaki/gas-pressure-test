#pragma once

#include "G4ThreeVector.hh"
#include "G4UserEventAction.hh"

// Collects the per event quantities filled by SteppingAction and writes one
// row of the events ntuple.
class EventAction : public G4UserEventAction {
 public:
  void BeginOfEventAction(const G4Event* event) override;
  void EndOfEventAction(const G4Event* event) override;

  void AddEdep(G4double edep) { fEdepTotal += edep; }
  void AddTrackLength(G4double length) { fTrackLength += length; }

  // Called for every primary step inside the gas; the last call wins.
  void SetEndPoint(const G4ThreeVector& position, G4double time,
                   G4double kineticEnergy, G4bool exited);

 private:
  G4double fTrackLength = 0.;
  G4double fEdepTotal = 0.;
  G4ThreeVector fEnd;
  G4double fTEnd = 0.;
  G4double fEExit = 0.;
  G4bool fExited = false;
};
