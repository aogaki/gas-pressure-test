#pragma once

#include <memory>

#include "G4VUserPrimaryGeneratorAction.hh"

class G4GeneralParticleSource;

// General particle source with the defaults of TODO/01: 5.5 MeV alpha starting
// at (0, 0, -250 mm) and travelling along +z. Everything can be overridden with
// the /gps/ commands in the macro.
class PrimaryGeneratorAction : public G4VUserPrimaryGeneratorAction {
 public:
  PrimaryGeneratorAction();
  ~PrimaryGeneratorAction() override;

  void GeneratePrimaries(G4Event* event) override;

  G4GeneralParticleSource& Source() { return *fSource; }

 private:
  std::unique_ptr<G4GeneralParticleSource> fSource;
};
