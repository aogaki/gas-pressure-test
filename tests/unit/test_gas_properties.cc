#include <gtest/gtest.h>

#include <stdexcept>

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
