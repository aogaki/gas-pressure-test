#include "PrimaryGeneratorAction.hh"

#include "G4Alpha.hh"
#include "G4GeneralParticleSource.hh"
#include "G4SPSAngDistribution.hh"
#include "G4SPSEneDistribution.hh"
#include "G4SPSPosDistribution.hh"
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"

PrimaryGeneratorAction::PrimaryGeneratorAction()
    : fSource(std::make_unique<G4GeneralParticleSource>()) {
  G4SingleParticleSource* source = fSource->GetCurrentSource();
  source->SetParticleDefinition(G4Alpha::Definition());
  source->GetEneDist()->SetEnergyDisType("Mono");
  source->GetEneDist()->SetMonoEnergy(5.5 * MeV);
  source->GetPosDist()->SetPosDisType("Point");
  source->GetPosDist()->SetCentreCoords(G4ThreeVector(0., 0., -250. * mm));
  source->GetAngDist()->SetAngDistType("planar");
  source->GetAngDist()->SetParticleMomentumDirection(
      G4ThreeVector(0., 0., 1.));
}

PrimaryGeneratorAction::~PrimaryGeneratorAction() = default;

void PrimaryGeneratorAction::GeneratePrimaries(G4Event* event) {
  fSource->GeneratePrimaryVertex(event);
}
