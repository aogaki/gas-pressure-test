#include "TpcMessenger.hh"

#include <stdexcept>

#include "G4ApplicationState.hh"
#include "G4GenericMessenger.hh"
#include "G4ios.hh"
#include "GasProperties.hh"
#include "TpcConfig.hh"

TpcMessenger::TpcMessenger(TpcConfig& config)
    : fConfig(config),
      fMessenger(std::make_unique<G4GenericMessenger>(this, "/tpc/",
                                                      "TPC run settings")) {
  // PreInit only: the geometry and the step limit are built once, by
  // /run/initialize, so a later change would only reach the "run" ntuple and
  // leave the material it describes untouched (TODO/09 R1, R2).
  fMessenger->DeclareMethod("gas", &TpcMessenger::SetGas,
                            "Gas: He, Ar, CO2 or a mixture such as"
                            " He-90-CO2-10 (before /run/initialize).")
      .SetStates(G4State_PreInit);
  fMessenger->DeclareMethod("pressure", &TpcMessenger::SetPressure,
                            "Gas pressure in mbar (before /run/initialize).")
      .SetStates(G4State_PreInit);
  fMessenger->DeclareMethod("hits", &TpcMessenger::SetHits,
                            "Write the hits ntuple and limit steps to 1 mm"
                            " (before /run/initialize).")
      .SetStates(G4State_PreInit);
}

TpcMessenger::~TpcMessenger() = default;

void TpcMessenger::SetGas(G4String gas) {
  try {
    ParseGasSpec(gas);  // Validates the gas specification.
  } catch (const std::invalid_argument& error) {
    G4ExceptionDescription message;
    message << "Bad gas '" << gas << "' (" << error.what()
            << "). Supported: He, Ar, CO2, and mixtures such as"
               " 'He-90-CO2-10'.";
    G4Exception("TpcMessenger::SetGas()", "tpc0001", FatalException, message);
  }
  fConfig.gas = gas;
}

void TpcMessenger::SetPressure(G4double pressureMbar) {
  if (!(pressureMbar > 0.)) {
    G4ExceptionDescription message;
    message << "Pressure must be positive, got " << pressureMbar << " mbar.";
    G4Exception("TpcMessenger::SetPressure()", "tpc0002", FatalException,
                message);
  }
  fConfig.pressureMbar = pressureMbar;
}

void TpcMessenger::SetHits(G4bool hits) { fConfig.hits = hits; }
