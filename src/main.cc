#include <fstream>
#include <iostream>
#include <memory>

#include "ActionInitialization.hh"
#include "DetectorConstruction.hh"
#include "G4RunManagerFactory.hh"
#include "G4UImanager.hh"
#include "PhysicsList.hh"
#include "TpcConfig.hh"
#include "TpcMessenger.hh"

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: gas-pressure-test <macro file>" << std::endl;
    return 1;
  }
  {
    std::ifstream macro(argv[1]);
    if (!macro) {
      std::cerr << "cannot open macro file: " << argv[1] << std::endl;
      return 1;
    }
  }

  const std::unique_ptr<G4RunManager> runManager(
      G4RunManagerFactory::CreateRunManager(G4RunManagerType::Serial));

  // Declared after the run manager so that they are destroyed before it: the
  // messenger removes its commands from the UI manager, which the run manager
  // owns.
  TpcConfig config;
  TpcMessenger messenger(config);

  auto* detector = new DetectorConstruction(config);
  runManager->SetUserInitialization(detector);
  runManager->SetUserInitialization(new PhysicsList());
  runManager->SetUserInitialization(new ActionInitialization(config, *detector));

  const G4int status = G4UImanager::GetUIpointer()->ApplyCommand(
      G4String("/control/execute ") + argv[1]);
  return status == 0 ? 0 : 1;
}
