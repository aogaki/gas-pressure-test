#pragma once

#include <memory>

#include "G4String.hh"
#include "G4Types.hh"

class G4GenericMessenger;
struct TpcConfig;

// Defines the three application specific UI commands:
//   /tpc/gas <He|Ar|CO2>, /tpc/pressure <mbar>, /tpc/hits <true|false>
class TpcMessenger {
 public:
  explicit TpcMessenger(TpcConfig& config);
  ~TpcMessenger();

 private:
  void SetGas(G4String gas);
  void SetPressure(G4double pressureMbar);
  void SetHits(G4bool hits);

  TpcConfig& fConfig;
  std::unique_ptr<G4GenericMessenger> fMessenger;
};
