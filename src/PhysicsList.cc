#include "PhysicsList.hh"

#include "G4EmStandardPhysics_option4.hh"
#include "G4StepLimiterPhysics.hh"
#include "G4SystemOfUnits.hh"

PhysicsList::PhysicsList() {
  // Assigned directly (as the Geant4 reference physics lists do): the setter
  // needs the default region, which only exists once the kernel is up.
  // 10 m: delta rays and fluorescence X-rays are not tracked as separate
  // particles, their energy stays on the alpha step instead, see TODO/01.
  defaultCutValue = 10. * m;
  RegisterPhysics(new G4EmStandardPhysics_option4());
  RegisterPhysics(new G4StepLimiterPhysics());
}
