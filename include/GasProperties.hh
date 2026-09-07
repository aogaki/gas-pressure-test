#pragma once

#include <string>

// Pure helpers, free of any Geant4 dependency.

// NIST material name of a supported gas ("He", "Ar", "CO2").
// Throws std::invalid_argument for an unknown gas.
std::string GasNistName(const std::string& gas);

// Ideal-gas density [g/cm3] at 20 degC: rho_NIST * pressureMbar / 1013.25.
// Throws std::invalid_argument for an unknown gas or a non-positive pressure.
double GasDensity(const std::string& gas, double pressureMbar);

// Shortest "%g" style representation of a number.
std::string FormatNumber(double value);

// Automatic output file name (no extension). A numeric energy tag gets the
// "MeV" unit appended, a distribution name (e.g. "Lin") is used as is.
std::string OutputFileName(const std::string& gas, double pressureMbar,
                           const std::string& energyTag);
