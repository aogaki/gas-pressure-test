#include "EventAction.hh"

#include "G4AnalysisManager.hh"
#include "G4Event.hh"
#include "G4PrimaryParticle.hh"
#include "G4PrimaryVertex.hh"
#include "G4SystemOfUnits.hh"
#include "Ntuples.hh"

void EventAction::BeginOfEventAction(const G4Event*) {
  fTrackLength = 0.;
  fEdepTotal = 0.;
  fEnd = G4ThreeVector();
  fTEnd = 0.;
  fEExit = 0.;
  fExited = false;
}

void EventAction::SetEndPoint(const G4ThreeVector& position, G4double time,
                              G4double kineticEnergy, G4bool exited) {
  fEnd = position;
  fTEnd = time;
  fEExit = kineticEnergy;
  fExited = exited;
}

void EventAction::EndOfEventAction(const G4Event* event) {
  const G4PrimaryVertex* vertex = event->GetPrimaryVertex();
  const G4PrimaryParticle* primary =
      (vertex != nullptr) ? vertex->GetPrimary() : nullptr;
  const G4ThreeVector origin =
      (vertex != nullptr)
          ? G4ThreeVector(vertex->GetX0(), vertex->GetY0(), vertex->GetZ0())
          : G4ThreeVector();
  const G4ThreeVector direction =
      (primary != nullptr) ? primary->GetMomentumDirection() : G4ThreeVector();
  const G4double e0 = (primary != nullptr) ? primary->GetKineticEnergy() : 0.;

  // An alpha that never entered the gas leaves no step to set the end point
  // with. The source sits on the entrance plane (z = -250 mm), so every -z
  // direction is such an alpha; recording it as "left at the entrance point
  // with all of its energy" keeps the range and the contained fraction of
  // range_summary.C right (TODO/09 R5).
  if (fTrackLength == 0.) {
    fEnd = origin;
    fTEnd = 0.;
    fEExit = e0;
    fExited = true;
  }

  auto* analysis = G4AnalysisManager::Instance();
  analysis->FillNtupleIColumn(ntuple::kEvents, ntuple::kEventID,
                              event->GetEventID());
  analysis->FillNtupleDColumn(ntuple::kEvents, ntuple::kE0, e0 / MeV);
  analysis->FillNtupleDColumn(ntuple::kEvents, ntuple::kX0, origin.x() / mm);
  analysis->FillNtupleDColumn(ntuple::kEvents, ntuple::kY0, origin.y() / mm);
  analysis->FillNtupleDColumn(ntuple::kEvents, ntuple::kZ0, origin.z() / mm);
  analysis->FillNtupleDColumn(ntuple::kEvents, ntuple::kDx0, direction.x());
  analysis->FillNtupleDColumn(ntuple::kEvents, ntuple::kDy0, direction.y());
  analysis->FillNtupleDColumn(ntuple::kEvents, ntuple::kDz0, direction.z());
  analysis->FillNtupleDColumn(ntuple::kEvents, ntuple::kTrackLength,
                              fTrackLength / mm);
  analysis->FillNtupleDColumn(ntuple::kEvents, ntuple::kXEnd, fEnd.x() / mm);
  analysis->FillNtupleDColumn(ntuple::kEvents, ntuple::kYEnd, fEnd.y() / mm);
  analysis->FillNtupleDColumn(ntuple::kEvents, ntuple::kZEnd, fEnd.z() / mm);
  analysis->FillNtupleDColumn(ntuple::kEvents, ntuple::kTEnd, fTEnd / ns);
  analysis->FillNtupleDColumn(ntuple::kEvents, ntuple::kEExit, fEExit / MeV);
  analysis->FillNtupleIColumn(ntuple::kEvents, ntuple::kExited,
                              fExited ? 1 : 0);
  analysis->FillNtupleDColumn(ntuple::kEvents, ntuple::kEdepTotal,
                              fEdepTotal / MeV);
  analysis->AddNtupleRow(ntuple::kEvents);
}
