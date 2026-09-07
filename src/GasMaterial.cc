#include "GasMaterial.hh"

#include <vector>

#include "G4NistManager.hh"
#include "G4SystemOfUnits.hh"
#include "GasProperties.hh"

namespace {

// Unique material name for one (gas, pressure) combination.
G4String MaterialName(const G4String& gas, G4double pressureMbar) {
  return gas + "_" + FormatNumber(pressureMbar) + "mbar";
}

// Room temperature used throughout, see TODO/01.
constexpr G4double kTemperature = 293.15 * kelvin;

}  // namespace

G4Material* BuildGasMaterial(const G4String& gas, G4double pressureMbar) {
  const G4String name = MaterialName(gas, pressureMbar);
  if (G4Material* existing = G4Material::GetMaterial(name, false)) {
    return existing;
  }
  const std::vector<GasComponent> components = ParseGasSpec(gas);
  const G4double density = GasDensity(gas, pressureMbar) * (g / cm3);
  const G4double pressure = pressureMbar * 1.0e-3 * bar;
  G4NistManager* nist = G4NistManager::Instance();

  // A single gas keeps the NIST material as its base material, so that the
  // ASTAR stopping power tables are still found by their material identity.
  if (components.size() == 1) {
    return nist->BuildMaterialWithNewDensity(name,
                                             GasNistName(components[0].name),
                                             density, kTemperature, pressure);
  }

  // Mass fractions of an ideal-gas mixture (TODO/05):
  // w_i = f_i rho_i,NIST / sum_j(f_j rho_j,NIST).
  G4double total = 0.;
  for (const GasComponent& component : components) {
    total += component.fraction * GasNistDensity(component.name);
  }
  auto* material =
      new G4Material(name, density, static_cast<G4int>(components.size()),
                     kStateGas, kTemperature, pressure);
  for (const GasComponent& component : components) {
    material->AddMaterial(
        nist->FindOrBuildMaterial(GasNistName(component.name)),
        component.fraction * GasNistDensity(component.name) / total);
  }
  return material;
}
