#include "PhysicsList.hh"

#include "G4EmStandardPhysics_option4.hh"
#include "G4StepLimiterPhysics.hh"
#include "G4SystemOfUnits.hh"

PhysicsList::PhysicsList() {
  // Assigned directly (as the Geant4 reference physics lists do): the setter
  // needs the default region, which only exists once the kernel is up.
  // 10 m: no delta ray is produced, because the electron production
  // threshold is 172.6 keV there, far above the 3 keV a 5.5 MeV alpha can
  // hand to an electron; their energy stays on the alpha step instead, see
  // TODO/01. Fluorescence X-rays are absent for another reason: PIXE is off
  // in option4 by default.
  defaultCutValue = 10. * m;
  RegisterPhysics(new G4EmStandardPhysics_option4());
  RegisterPhysics(new G4StepLimiterPhysics());
}
