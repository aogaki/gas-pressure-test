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

// --- AT-M1: gas mixtures ---------------------------------------------------

TEST(GasMaterialTest, BuildsHeCO2Mixture) {
  G4Material* material = BuildGasMaterial("He-90-CO2-10", 200.);
  ASSERT_NE(material, nullptr);
  EXPECT_NEAR(material->GetDensity() / (g / cm3), 6.5907e-5, 6.5907e-5 * 1e-3);
  EXPECT_EQ(material->GetState(), kStateGas);
  // He, C and O.
  EXPECT_EQ(material->GetNumberOfElements(), 3u);
  EXPECT_EQ(material->GetName(), "He-90-CO2-10_200mbar");
}

TEST(GasMaterialTest, MixtureMassFractionsFollowThePartialDensities) {
  G4Material* material = BuildGasMaterial("He-90-CO2-10", 1013.25);
  ASSERT_NE(material, nullptr);
  // w_He = 0.9 * rho_He / rho_mix, so helium carries 44.83 % of the mass.
  const G4double* fractions = material->GetFractionVector();
  G4double helium = 0.;
  for (size_t i = 0; i < material->GetNumberOfElements(); ++i) {
    if (material->GetElement(i)->GetZ() == 2.) helium = fractions[i];
  }
  EXPECT_NEAR(helium, 0.4483, 0.4483 * 1e-3);
}

TEST(GasMaterialTest, SameMixtureReusesTheSameMaterial) {
  G4Material* first = BuildGasMaterial("Ar-90-CO2-10", 200.);
  G4Material* second = BuildGasMaterial("Ar-90-CO2-10", 200.);
  EXPECT_EQ(first, second);
}
