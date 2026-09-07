#pragma once

#include <string>
#include <vector>

// Pure helpers, free of any Geant4 dependency.

// One component of a gas specification.
struct GasComponent {
  std::string name;  // "He", "Ar" or "CO2"
  double fraction;   // volume (molar) percentage
};

// Parses a gas specification "name-percent-name-percent..." such as
// "He-90-CO2-10". A bare name ("He") means 100 % of that gas. The order of
// the components is kept. Throws std::invalid_argument when a name is
// unknown, a percentage is not a positive number, the percentages do not add
// up to 100, or there are more than six components (the Magboltz limit).
std::vector<GasComponent> ParseGasSpec(const std::string& spec);

// NIST material name of a single gas ("He", "Ar", "CO2").
// Throws std::invalid_argument for an unknown gas.
std::string GasNistName(const std::string& gas);

// NIST density [g/cm3] of a single gas at 20 degC and 1 atm.
// Throws std::invalid_argument for an unknown gas.
double GasNistDensity(const std::string& gas);

// Ideal-gas density [g/cm3] at 20 degC of a gas specification: the sum of the
// partial pressures, (P / 1013.25) * sum(f_i / 100 * rho_i,NIST).
// Throws std::invalid_argument for a bad specification or a non-positive
// pressure.
double GasDensity(const std::string& gas, double pressureMbar);

// Shortest "%g" style representation of a number.
std::string FormatNumber(double value);

// Automatic output file name (no extension). A numeric energy tag gets the
// "MeV" unit appended, a distribution name (e.g. "Lin") is used as is.
std::string OutputFileName(const std::string& gas, double pressureMbar,
                           const std::string& energyTag);
