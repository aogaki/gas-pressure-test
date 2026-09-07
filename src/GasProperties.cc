#include "GasProperties.hh"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
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

// Magboltz takes at most six gases, so a specification may not have more.
constexpr std::size_t kMaxComponents = 6;

// Splits "He-90-CO2-10" into {"He", "90", "CO2", "10"}. An empty field is
// kept, so that a malformed specification is rejected further down.
std::vector<std::string> SplitOnDash(const std::string& spec) {
  std::vector<std::string> tokens;
  std::size_t begin = 0;
  for (std::size_t i = 0; i <= spec.size(); ++i) {
    if (i == spec.size() || spec[i] == '-') {
      tokens.push_back(spec.substr(begin, i - begin));
      begin = i + 1;
    }
  }
  return tokens;
}

// strtod() with a "the whole token was a number" check.
double ToPercentage(const std::string& token, const std::string& spec) {
  char* end = nullptr;
  const double value = std::strtod(token.c_str(), &end);
  if (end == token.c_str() || *end != '\0') {
    throw std::invalid_argument("not a percentage: '" + token + "' in " + spec);
  }
  return value;
}

}  // namespace

std::vector<GasComponent> ParseGasSpec(const std::string& spec) {
  const std::vector<std::string> tokens = SplitOnDash(spec);
  std::vector<GasComponent> components;
  if (tokens.size() == 1) {
    components.push_back({tokens[0], 100.});
  } else {
    if (tokens.size() % 2 != 0) {
      throw std::invalid_argument(
          "gas specification needs name-percent pairs: " + spec);
    }
    for (std::size_t i = 0; i < tokens.size(); i += 2) {
      components.push_back({tokens[i], ToPercentage(tokens[i + 1], spec)});
    }
  }
  if (components.size() > kMaxComponents) {
    throw std::invalid_argument("at most six components: " + spec);
  }

  double sum = 0.;
  for (const GasComponent& component : components) {
    FindGas(component.name);  // Validates the name.
    if (!(component.fraction > 0.)) {
      throw std::invalid_argument("percentages must be positive: " + spec);
    }
    sum += component.fraction;
  }
  if (std::fabs(sum - 100.) > 1e-6) {
    throw std::invalid_argument("percentages must add up to 100: " + spec);
  }
  return components;
}

std::string GasNistName(const std::string& gas) { return FindGas(gas).nistName; }

double GasNistDensity(const std::string& gas) { return FindGas(gas).nistDensity; }

double GasDensity(const std::string& gas, double pressureMbar) {
  if (!(pressureMbar > 0.)) {
    throw std::invalid_argument("pressure must be positive");
  }
  double density = 0.;
  for (const GasComponent& component : ParseGasSpec(gas)) {
    // 0.01 * 100 is exactly 1, so a single gas keeps its NIST density.
    density += 0.01 * component.fraction * FindGas(component.name).nistDensity;
  }
  return density * pressureMbar / kReferencePressureMbar;
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
