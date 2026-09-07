#include "SteppingAction.hh"

#include "DetectorConstruction.hh"
#include "EventAction.hh"
#include "G4AnalysisManager.hh"
#include "G4Event.hh"
#include "G4EventManager.hh"
#include "G4LogicalVolume.hh"
#include "G4Step.hh"
#include "G4SystemOfUnits.hh"
#include "G4Track.hh"
#include "G4VPhysicalVolume.hh"
#include "Ntuples.hh"
#include "TpcConfig.hh"

SteppingAction::SteppingAction(const TpcConfig& config,
                               const DetectorConstruction& detector,
                               EventAction& eventAction)
    : fConfig(config), fDetector(detector), fEventAction(eventAction) {}

void SteppingAction::UserSteppingAction(const G4Step* step) {
  const G4LogicalVolume* gas = fDetector.GasLogicalVolume();
  const G4VPhysicalVolume* preVolume =
      step->GetPreStepPoint()->GetTouchableHandle()->GetVolume();
  if (preVolume == nullptr || preVolume->GetLogicalVolume() != gas) return;

  const G4double edep = step->GetTotalEnergyDeposit();
  fEventAction.AddEdep(edep);

  const G4StepPoint* pre = step->GetPreStepPoint();
  const G4StepPoint* post = step->GetPostStepPoint();
  const G4Track* track = step->GetTrack();

  if (fConfig.hits && edep > 0.) {
    auto* analysis = G4AnalysisManager::Instance();
    const G4int eventID = G4EventManager::GetEventManager()
                              ->GetConstCurrentEvent()
                              ->GetEventID();
    // The middle of the step, in space and in time: the ionisation is spread
    // along the 1 mm step, and the end point would shift the whole charge
    // distribution by half a step and comb it at the step pitch
    // (TODO/09 R11).
    const G4ThreeVector hit =
        0.5 * (pre->GetPosition() + post->GetPosition());
    const G4double hitTime =
        0.5 * (pre->GetGlobalTime() + post->GetGlobalTime());
    analysis->FillNtupleIColumn(ntuple::kHits, ntuple::kHitEventID, eventID);
    analysis->FillNtupleIColumn(ntuple::kHits, ntuple::kHitTrackID,
                                track->GetTrackID());
    analysis->FillNtupleIColumn(
        ntuple::kHits, ntuple::kHitPdg,
        track->GetParticleDefinition()->GetPDGEncoding());
    analysis->FillNtupleDColumn(ntuple::kHits, ntuple::kHitX, hit.x() / mm);
    analysis->FillNtupleDColumn(ntuple::kHits, ntuple::kHitY, hit.y() / mm);
    analysis->FillNtupleDColumn(ntuple::kHits, ntuple::kHitZ, hit.z() / mm);
    analysis->FillNtupleDColumn(ntuple::kHits, ntuple::kHitT, hitTime / ns);
    analysis->FillNtupleDColumn(ntuple::kHits, ntuple::kHitEdep, edep / MeV);
    analysis->FillNtupleDColumn(ntuple::kHits, ntuple::kHitStepLength,
                                step->GetStepLength() / mm);
    analysis->AddNtupleRow(ntuple::kHits);
  }

  // Only the primary particle contributes to the range measurement.
  if (track->GetTrackID() != 1 || track->GetParentID() != 0) return;

  fEventAction.AddTrackLength(step->GetStepLength());

  const G4VPhysicalVolume* postVolume = post->GetTouchableHandle()->GetVolume();
  const G4bool leftGas =
      (postVolume == nullptr) || (postVolume->GetLogicalVolume() != gas);
  fEventAction.SetEndPoint(post->GetPosition(), post->GetGlobalTime(),
                           leftGas ? post->GetKineticEnergy() : 0., leftGas);
}
