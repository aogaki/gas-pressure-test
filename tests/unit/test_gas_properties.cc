#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include "GasProperties.hh"

TEST(GasNistNameTest, KnownGases) {
  EXPECT_EQ(GasNistName("He"), "G4_He");
  EXPECT_EQ(GasNistName("Ar"), "G4_Ar");
  EXPECT_EQ(GasNistName("CO2"), "G4_CARBON_DIOXIDE");
}

TEST(GasNistNameTest, UnknownGasThrows) {
  EXPECT_THROW(GasNistName("Xe"), std::invalid_argument);
  EXPECT_THROW(GasNistName(""), std::invalid_argument);
  EXPECT_THROW(GasNistName("he"), std::invalid_argument);
}

TEST(GasDensityTest, ScalesLinearlyWithPressure) {
  // rho(P) = rho_NIST * P / 1013.25 mbar, in g/cm3.
  EXPECT_NEAR(GasDensity("He", 200.), 3.2829e-5, 3.2829e-5 * 1e-4);
  EXPECT_NEAR(GasDensity("Ar", 200.), 3.2806e-4, 3.2806e-4 * 1e-4);
  EXPECT_NEAR(GasDensity("CO2", 200.), 3.6361e-4, 3.6361e-4 * 1e-4);
}

TEST(GasDensityTest, AtOneAtmosphereEqualsNistDensity) {
  EXPECT_NEAR(GasDensity("He", 1013.25), 1.66322e-4, 1.66322e-4 * 1e-9);
  EXPECT_NEAR(GasDensity("Ar", 1013.25), 1.66201e-3, 1.66201e-3 * 1e-9);
  EXPECT_NEAR(GasDensity("CO2", 1013.25), 1.84212e-3, 1.84212e-3 * 1e-9);
}

TEST(GasDensityTest, InvalidArgumentsThrow) {
  EXPECT_THROW(GasDensity("Ar", 0.), std::invalid_argument);
  EXPECT_THROW(GasDensity("Ar", -1.), std::invalid_argument);
  EXPECT_THROW(GasDensity("Xe", 200.), std::invalid_argument);
}

TEST(FormatNumberTest, ShortestRepresentation) {
  EXPECT_EQ(FormatNumber(200.), "200");
  EXPECT_EQ(FormatNumber(0.3), "0.3");
  EXPECT_EQ(FormatNumber(1013.25), "1013.25");
  EXPECT_EQ(FormatNumber(5.5), "5.5");
}

TEST(OutputFileNameTest, NumericEnergyGetsMeVSuffix) {
  EXPECT_EQ(OutputFileName("He", 200., "0.3"), "He_200mbar_0.3MeV");
  EXPECT_EQ(OutputFileName("Ar", 1013.25, "5.5"), "Ar_1013.25mbar_5.5MeV");
}

TEST(OutputFileNameTest, DistributionNameIsUsedAsIs) {
  // A non-Mono GPS energy distribution contributes its name, without a unit.
  EXPECT_EQ(OutputFileName("Ar", 200., "Lin"), "Ar_200mbar_Lin");
}

// --- AT-M1: gas mixtures ---------------------------------------------------

TEST(ParseGasSpecTest, BareNameIsHundredPercent) {
  const std::vector<GasComponent> components = ParseGasSpec("He");
  ASSERT_EQ(components.size(), 1u);
  EXPECT_EQ(components[0].name, "He");
  EXPECT_DOUBLE_EQ(components[0].fraction, 100.);
}

TEST(ParseGasSpecTest, ExplicitHundredPercentIsTheSame) {
  const std::vector<GasComponent> components = ParseGasSpec("Ar-100");
  ASSERT_EQ(components.size(), 1u);
  EXPECT_EQ(components[0].name, "Ar");
  EXPECT_DOUBLE_EQ(components[0].fraction, 100.);
}

TEST(ParseGasSpecTest, TwoComponents) {
  const std::vector<GasComponent> components = ParseGasSpec("He-90-CO2-10");
  ASSERT_EQ(components.size(), 2u);
  EXPECT_EQ(components[0].name, "He");
  EXPECT_DOUBLE_EQ(components[0].fraction, 90.);
  EXPECT_EQ(components[1].name, "CO2");
  EXPECT_DOUBLE_EQ(components[1].fraction, 10.);
}

TEST(ParseGasSpecTest, FractionalPercentages) {
  const std::vector<GasComponent> components = ParseGasSpec("He-92.5-CO2-7.5");
  ASSERT_EQ(components.size(), 2u);
  EXPECT_DOUBLE_EQ(components[0].fraction, 92.5);
  EXPECT_DOUBLE_EQ(components[1].fraction, 7.5);
}

TEST(ParseGasSpecTest, ThreeComponentsKeepTheirOrder) {
  const std::vector<GasComponent> components = ParseGasSpec("CO2-10-Ar-20-He-70");
  ASSERT_EQ(components.size(), 3u);
  EXPECT_EQ(components[0].name, "CO2");
  EXPECT_EQ(components[1].name, "Ar");
  EXPECT_EQ(components[2].name, "He");
}

TEST(ParseGasSpecTest, MalformedSpecificationsThrow) {
  EXPECT_THROW(ParseGasSpec("He-90"), std::invalid_argument);        // sum 90
  EXPECT_THROW(ParseGasSpec("He-90-CO2-20"), std::invalid_argument); // sum 110
  EXPECT_THROW(ParseGasSpec("He-0-CO2-100"), std::invalid_argument); // zero
  EXPECT_THROW(ParseGasSpec("Xe-100"), std::invalid_argument);       // unknown
  EXPECT_THROW(ParseGasSpec("He-CO2-90-10"), std::invalid_argument); // not a number
  EXPECT_THROW(ParseGasSpec("He-90-CO2"), std::invalid_argument);    // odd count
  EXPECT_THROW(ParseGasSpec(""), std::invalid_argument);
}

TEST(ParseGasSpecTest, MoreThanSixComponentsThrow) {
  // Magboltz takes at most six gases. Only three names are known, so a
  // specification that long always repeats one, which is refused as well.
  EXPECT_THROW(ParseGasSpec("He-50-Ar-10-CO2-10-He-10-Ar-10-CO2-5-He-5"),
               std::invalid_argument);
}

TEST(ParseGasSpecTest, RepeatedComponentThrows) {
  EXPECT_THROW(ParseGasSpec("He-50-Ar-40-He-10"), std::invalid_argument);
  EXPECT_THROW(ParseGasSpec("CO2-50-CO2-50"), std::invalid_argument);
  EXPECT_NO_THROW(ParseGasSpec("He-50-Ar-40-CO2-10"));
}

TEST(GasDensityTest, MixtureIsThePartialPressureSum) {
  // rho = (P / 1013.25) * sum(f_i / 100 * rho_i,NIST), see TODO/05.
  EXPECT_NEAR(GasDensity("He-90-CO2-10", 1013.25), 3.3390e-4, 3.3390e-4 * 1e-3);
  EXPECT_NEAR(GasDensity("He-90-CO2-10", 200.), 6.5907e-5, 6.5907e-5 * 1e-3);
  EXPECT_NEAR(GasDensity("Ar-90-CO2-10", 1013.25), 1.6800e-3, 1.6800e-3 * 1e-3);
}

TEST(GasNistDensityTest, ComponentDensities) {
  EXPECT_DOUBLE_EQ(GasNistDensity("He"), 1.66322e-4);
  EXPECT_DOUBLE_EQ(GasNistDensity("Ar"), 1.66201e-3);
  EXPECT_DOUBLE_EQ(GasNistDensity("CO2"), 1.84212e-3);
  EXPECT_THROW(GasNistDensity("Xe"), std::invalid_argument);
}
