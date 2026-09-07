#include <gtest/gtest.h>

#include "G4Event.hh"
#include "G4GeneralParticleSource.hh"
#include "G4PrimaryParticle.hh"
#include "G4PrimaryVertex.hh"
#include "G4SystemOfUnits.hh"
#include "G4ParticleTable.hh"
#include "PrimaryGeneratorAction.hh"

TEST(PrimaryGeneratorActionTest, Defaults) {
  // Outside a run manager nothing declares the particle table ready, which
  // G4VUserPrimaryGeneratorAction insists on.
  G4ParticleTable::GetParticleTable()->SetReadiness();
  PrimaryGeneratorAction generator;
  G4Event event;
  generator.GeneratePrimaries(&event);

  const G4PrimaryVertex* vertex = event.GetPrimaryVertex();
  ASSERT_NE(vertex, nullptr);
  EXPECT_DOUBLE_EQ(vertex->GetX0(), 0.);
  EXPECT_DOUBLE_EQ(vertex->GetY0(), 0.);
  EXPECT_DOUBLE_EQ(vertex->GetZ0(), -250. * mm);
  EXPECT_DOUBLE_EQ(vertex->GetT0(), 0.);

  const G4PrimaryParticle* primary = vertex->GetPrimary();
  ASSERT_NE(primary, nullptr);
  EXPECT_EQ(primary->GetParticleDefinition()->GetParticleName(), "alpha");
  EXPECT_NEAR(primary->GetKineticEnergy() / MeV, 5.5, 1e-9);
  EXPECT_NEAR(primary->GetMomentumDirection().x(), 0., 1e-12);
  EXPECT_NEAR(primary->GetMomentumDirection().y(), 0., 1e-12);
  EXPECT_NEAR(primary->GetMomentumDirection().z(), 1., 1e-12);
}
