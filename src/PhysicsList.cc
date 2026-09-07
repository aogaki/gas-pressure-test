#include "PhysicsList.hh"

#include "G4EmStandardPhysics_option4.hh"
#include "G4StepLimiterPhysics.hh"
#include "G4SystemOfUnits.hh"

PhysicsList::PhysicsList() {
  // Assigned directly (as the Geant4 reference physics lists do): the setter
  // needs the default region, which only exists once the kernel is up.
  defaultCutValue = 0.7 * mm;
  RegisterPhysics(new G4EmStandardPhysics_option4());
  auto* stepLimiter = new G4StepLimiterPhysics();
  // Neutrals too, so that the 1 mm limit of /tpc/hits really holds for every
  // row of the hits ntuple (a fluorescence gamma can otherwise fly further
  // before depositing its energy).
  stepLimiter->SetApplyToAll(true);
  RegisterPhysics(stepLimiter);
}
