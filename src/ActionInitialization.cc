#include "ActionInitialization.hh"

#include "EventAction.hh"
#include "PrimaryGeneratorAction.hh"
#include "RunAction.hh"
#include "SteppingAction.hh"

ActionInitialization::ActionInitialization(const TpcConfig& config,
                                           DetectorConstruction& detector)
    : fConfig(config), fDetector(detector) {}

void ActionInitialization::Build() const {
  auto* generator = new PrimaryGeneratorAction();
  SetUserAction(generator);
  SetUserAction(new RunAction(fConfig, *generator));
  auto* eventAction = new EventAction();
  SetUserAction(eventAction);
  SetUserAction(new SteppingAction(fConfig, fDetector, *eventAction));
}
