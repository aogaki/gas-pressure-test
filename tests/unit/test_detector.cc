#include <gtest/gtest.h>

#include <cfloat>

#include "DetectorConstruction.hh"
#include "G4Alpha.hh"
#include "G4Box.hh"
#include "G4DynamicParticle.hh"
#include "G4Material.hh"
#include "G4LogicalVolume.hh"
#include "G4SystemOfUnits.hh"
#include "G4UImanager.hh"
#include "G4Track.hh"
#include "G4UserLimits.hh"
#include "G4VPhysicalVolume.hh"
#include "TpcConfig.hh"
#include "TpcMessenger.hh"

namespace {

// G4UserLimits needs a track to answer; its value does not depend on it.
G4Track& DummyTrack() {
  static G4Track track(
      new G4DynamicParticle(G4Alpha::Definition(), G4ThreeVector(0., 0., 1.),
                            5.5 * MeV),
      0., G4ThreeVector());
  return track;
}

}  // namespace

TEST(DetectorConstructionTest, BuildsWorldAndGasVolume) {
  TpcConfig config;
  config.gas = "Ar";
  config.pressureMbar = 200.;
  config.hits = false;

  DetectorConstruction detector(config);
  G4VPhysicalVolume* world = detector.Construct();
  ASSERT_NE(world, nullptr);

  const auto* worldBox = dynamic_cast<const G4Box*>(world->GetLogicalVolume()->GetSolid());
  ASSERT_NE(worldBox, nullptr);
  EXPECT_DOUBLE_EQ(worldBox->GetXHalfLength(), 500. * mm);
  EXPECT_DOUBLE_EQ(worldBox->GetYHalfLength(), 500. * mm);
  EXPECT_DOUBLE_EQ(worldBox->GetZHalfLength(), 500. * mm);
  EXPECT_EQ(world->GetLogicalVolume()->GetMaterial()->GetName(), "G4_Galactic");

  G4LogicalVolume* gas = detector.GasLogicalVolume();
  ASSERT_NE(gas, nullptr);
  const auto* gasBox = dynamic_cast<const G4Box*>(gas->GetSolid());
  ASSERT_NE(gasBox, nullptr);
  EXPECT_DOUBLE_EQ(gasBox->GetXHalfLength(), 100. * mm);
  EXPECT_DOUBLE_EQ(gasBox->GetYHalfLength(), 100. * mm);
  EXPECT_DOUBLE_EQ(gasBox->GetZHalfLength(), 250. * mm);
  EXPECT_NEAR(gas->GetMaterial()->GetDensity() / (g / cm3), 3.2806e-4,
              3.2806e-4 * 1e-4);

  // The gas volume is placed at the origin inside the world.
  ASSERT_EQ(world->GetLogicalVolume()->GetNoDaughters(), 1u);
  EXPECT_EQ(world->GetLogicalVolume()->GetDaughter(0)->GetTranslation(),
            G4ThreeVector());
}

TEST(DetectorConstructionTest, StepLimitFollowsHitsSetting) {
  TpcConfig config;
  config.hits = false;
  DetectorConstruction detector(config);
  detector.Construct();

  G4UserLimits* limits = detector.GasLogicalVolume()->GetUserLimits();
  ASSERT_NE(limits, nullptr);
  EXPECT_DOUBLE_EQ(limits->GetMaxAllowedStep(DummyTrack()), DBL_MAX);

  config.hits = true;
  detector.ApplyStepLimit();
  EXPECT_DOUBLE_EQ(limits->GetMaxAllowedStep(DummyTrack()), 1. * mm);
}

TEST(TpcMessengerTest, CommandsUpdateTheConfiguration) {
  TpcConfig config;
  TpcMessenger messenger(config);
  G4UImanager* ui = G4UImanager::GetUIpointer();

  EXPECT_EQ(ui->ApplyCommand("/tpc/gas He"), 0);
  EXPECT_EQ(config.gas, "He");
  EXPECT_EQ(ui->ApplyCommand("/tpc/pressure 200"), 0);
  EXPECT_DOUBLE_EQ(config.pressureMbar, 200.);
  EXPECT_EQ(ui->ApplyCommand("/tpc/hits true"), 0);
  EXPECT_TRUE(config.hits);
  EXPECT_EQ(ui->ApplyCommand("/tpc/hits false"), 0);
  EXPECT_FALSE(config.hits);
}
