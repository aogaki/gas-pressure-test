#include <gtest/gtest.h>

#include "G4Material.hh"
#include "G4NistManager.hh"
#include "G4SystemOfUnits.hh"
#include "GasMaterial.hh"
#include "GasProperties.hh"

namespace {

double NistDensityGperCm3(const char* name) {
  const G4Material* material =
      G4NistManager::Instance()->FindOrBuildMaterial(name);
  return material->GetDensity() / (g / cm3);
}

}  // namespace

TEST(GasMaterialTest, HardCodedNistDensitiesMatchGeant4) {
  // The pure GasDensity() table must agree with what Geant4 ships.
  EXPECT_NEAR(GasDensity("He", 1013.25), NistDensityGperCm3("G4_He"),
              1.66322e-4 * 1e-6);
  EXPECT_NEAR(GasDensity("Ar", 1013.25), NistDensityGperCm3("G4_Ar"),
              1.66201e-3 * 1e-6);
  EXPECT_NEAR(GasDensity("CO2", 1013.25),
              NistDensityGperCm3("G4_CARBON_DIOXIDE"), 1.84212e-3 * 1e-6);
}

TEST(GasMaterialTest, BuildsScaledArgonWithBaseMaterial) {
  G4Material* material = BuildGasMaterial("Ar", 200.);
  ASSERT_NE(material, nullptr);
  EXPECT_NEAR(material->GetDensity() / (g / cm3), 3.2806e-4, 3.2806e-4 * 1e-4);
  ASSERT_NE(material->GetBaseMaterial(), nullptr);
  EXPECT_EQ(material->GetBaseMaterial()->GetName(), "G4_Ar");
  EXPECT_EQ(material->GetState(), kStateGas);
}

TEST(GasMaterialTest, SameArgumentsReuseTheSameMaterial) {
  G4Material* first = BuildGasMaterial("He", 200.);
  G4Material* second = BuildGasMaterial("He", 200.);
  EXPECT_EQ(first, second);
}

TEST(GasMaterialTest, DifferentPressuresGiveDifferentMaterials) {
  G4Material* low = BuildGasMaterial("CO2", 200.);
  G4Material* high = BuildGasMaterial("CO2", 1013.25);
  EXPECT_NE(low, high);
  EXPECT_NEAR(high->GetDensity() / (g / cm3), 1.84212e-3, 1.84212e-3 * 1e-6);
}
