#include "RunAction.hh"

#include "DetectorConstruction.hh"
#include "G4AnalysisManager.hh"
#include "G4GeneralParticleSource.hh"
#include "G4SPSEneDistribution.hh"
#include "G4SystemOfUnits.hh"
#include "GasProperties.hh"
#include "Ntuples.hh"
#include "PrimaryGeneratorAction.hh"
#include "TpcConfig.hh"

RunAction::RunAction(const TpcConfig& config, DetectorConstruction& detector,
                     PrimaryGeneratorAction& generator)
    : fConfig(config), fDetector(detector), fGenerator(generator) {
  G4AnalysisManager::Instance()->SetDefaultFileType("root");
}

G4String RunAction::EnergyTag() const {
  G4SPSEneDistribution* energy =
      fGenerator.Source().GetCurrentSource()->GetEneDist();
  const G4String type = energy->GetEnergyDisType();
  if (type == "Mono") return FormatNumber(energy->GetMonoEnergy() / MeV);
  return type;
}

void RunAction::CreateNtuples() {
  auto* analysis = G4AnalysisManager::Instance();

  analysis->CreateNtuple("events", "One row per event");
  analysis->CreateNtupleIColumn("eventID");
  analysis->CreateNtupleDColumn("e0");
  analysis->CreateNtupleDColumn("x0");
  analysis->CreateNtupleDColumn("y0");
  analysis->CreateNtupleDColumn("z0");
  analysis->CreateNtupleDColumn("dx0");
  analysis->CreateNtupleDColumn("dy0");
  analysis->CreateNtupleDColumn("dz0");
  analysis->CreateNtupleDColumn("trackLength");
  analysis->CreateNtupleDColumn("xEnd");
  analysis->CreateNtupleDColumn("yEnd");
  analysis->CreateNtupleDColumn("zEnd");
  analysis->CreateNtupleDColumn("tEnd");
  analysis->CreateNtupleDColumn("eExit");
  analysis->CreateNtupleIColumn("exited");
  analysis->CreateNtupleDColumn("edepTotal");
  analysis->FinishNtuple();

  if (!fConfig.hits) return;

  analysis->CreateNtuple("hits", "One row per energy depositing step");
  analysis->CreateNtupleIColumn("eventID");
  analysis->CreateNtupleIColumn("trackID");
  analysis->CreateNtupleIColumn("pdg");
  analysis->CreateNtupleDColumn("x");
  analysis->CreateNtupleDColumn("y");
  analysis->CreateNtupleDColumn("z");
  analysis->CreateNtupleDColumn("t");
  analysis->CreateNtupleDColumn("edep");
  analysis->CreateNtupleDColumn("stepLength");
  analysis->FinishNtuple();
}

void RunAction::BeginOfRunAction(const G4Run*) {
  fDetector.ApplyStepLimit();

  auto* analysis = G4AnalysisManager::Instance();
  if (analysis->GetFileName().empty()) {
    // The ".root" is spelled out: the generated name contains dots (e.g.
    // "Ar_1013.25mbar_5.5MeV") and G4AnalysisManager would take the text after
    // the last dot as the file extension.
    analysis->SetFileName(
        OutputFileName(fConfig.gas, fConfig.pressureMbar, EnergyTag()) +
        ".root");
  }
  if (!fNtuplesCreated) {
    CreateNtuples();
    fNtuplesCreated = true;
  }
  analysis->OpenFile();
}

void RunAction::EndOfRunAction(const G4Run*) {
  auto* analysis = G4AnalysisManager::Instance();
  analysis->Write();
  analysis->CloseFile();
}
