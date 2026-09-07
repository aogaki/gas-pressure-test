#include "GasMaterial.hh"

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
  const G4double density = GasDensity(gas, pressureMbar) * (g / cm3);
  return G4NistManager::Instance()->BuildMaterialWithNewDensity(
      name, GasNistName(gas), density, kTemperature,
      pressureMbar * 1.0e-3 * bar);
}
