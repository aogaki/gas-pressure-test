#include "TpcMessenger.hh"

#include <stdexcept>

#include "G4GenericMessenger.hh"
#include "G4ios.hh"
#include "GasProperties.hh"
#include "TpcConfig.hh"

TpcMessenger::TpcMessenger(TpcConfig& config)
    : fConfig(config),
      fMessenger(std::make_unique<G4GenericMessenger>(this, "/tpc/",
                                                      "TPC run settings")) {
  fMessenger->DeclareMethod("gas", &TpcMessenger::SetGas,
                            "Gas: He, Ar or CO2 (before /run/initialize).");
  fMessenger->DeclareMethod("pressure", &TpcMessenger::SetPressure,
                            "Gas pressure in mbar (before /run/initialize).");
  fMessenger->DeclareMethod("hits", &TpcMessenger::SetHits,
                            "Write the hits ntuple and limit steps to 1 mm.");
}

TpcMessenger::~TpcMessenger() = default;

void TpcMessenger::SetGas(G4String gas) {
  try {
    GasNistName(gas);  // Validates the gas name.
  } catch (const std::invalid_argument&) {
    G4ExceptionDescription message;
    message << "Unknown gas '" << gas << "'. Supported: He, Ar, CO2.";
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
