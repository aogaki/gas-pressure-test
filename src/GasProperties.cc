#include "GasProperties.hh"

#include <cctype>
#include <cstdio>
#include <map>
#include <stdexcept>

namespace {

// Geant4 NIST densities [g/cm3] at 20 degC and 1 atm.
struct GasData {
  const char* nistName;
  double nistDensity;
};

const std::map<std::string, GasData>& GasTable() {
  static const std::map<std::string, GasData> table = {
      {"He", {"G4_He", 1.66322e-4}},
      {"Ar", {"G4_Ar", 1.66201e-3}},
      {"CO2", {"G4_CARBON_DIOXIDE", 1.84212e-3}},
  };
  return table;
}

const GasData& FindGas(const std::string& gas) {
  const auto it = GasTable().find(gas);
  if (it == GasTable().end()) {
    throw std::invalid_argument("unknown gas: " + gas);
  }
  return it->second;
}

constexpr double kReferencePressureMbar = 1013.25;

}  // namespace

std::string GasNistName(const std::string& gas) { return FindGas(gas).nistName; }

double GasDensity(const std::string& gas, double pressureMbar) {
  const GasData& data = FindGas(gas);
  if (!(pressureMbar > 0.)) {
    throw std::invalid_argument("pressure must be positive");
  }
  return data.nistDensity * pressureMbar / kReferencePressureMbar;
}

std::string FormatNumber(double value) {
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%g", value);
  return buffer;
}

std::string OutputFileName(const std::string& gas, double pressureMbar,
                           const std::string& energyTag) {
  const bool numeric =
      !energyTag.empty() &&
      (std::isdigit(static_cast<unsigned char>(energyTag[0])) != 0 ||
       energyTag[0] == '.' || energyTag[0] == '-' || energyTag[0] == '+');
  return gas + "_" + FormatNumber(pressureMbar) + "mbar_" + energyTag +
         (numeric ? "MeV" : "");
}
