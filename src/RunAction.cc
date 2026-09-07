#include "RunAction.hh"

#include "G4AnalysisManager.hh"
#include "G4GeneralParticleSource.hh"
#include "G4SPSEneDistribution.hh"
#include "G4SystemOfUnits.hh"
#include "GasProperties.hh"
#include "Ntuples.hh"
#include "PrimaryGeneratorAction.hh"
#include "TpcConfig.hh"

namespace {

bool EndsWithRoot(const G4String& name) {
  constexpr std::size_t kExtLength = 5;  // ".root"
  return name.size() >= kExtLength &&
        name.compare(name.size() - kExtLength, kExtLength, ".root") == 0;
}

}  // namespace

RunAction::RunAction(const TpcConfig& config,
                     PrimaryGeneratorAction& generator)
    : fConfig(config), fGenerator(generator) {
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

  if (fConfig.hits) {
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

  fRunNtupleId = analysis->CreateNtuple(
      "run", "One row with the gas, pressure and hits setting");
  analysis->CreateNtupleSColumn("gas");
  analysis->CreateNtupleDColumn("pressure");
  analysis->CreateNtupleIColumn("hits");
  analysis->FinishNtuple();
}

void RunAction::WriteRunNtuple() {
  auto* analysis = G4AnalysisManager::Instance();
  analysis->FillNtupleSColumn(fRunNtupleId, ntuple::kRunGas, fConfig.gas);
  analysis->FillNtupleDColumn(fRunNtupleId, ntuple::kRunPressure,
                              fConfig.pressureMbar);
  analysis->FillNtupleIColumn(fRunNtupleId, ntuple::kRunHits,
                              fConfig.hits ? 1 : 0);
  analysis->AddNtupleRow(fRunNtupleId);
}

void RunAction::BeginOfRunAction(const G4Run*) {
  if (fNtuplesCreated) {
    // The second run would reopen the same file with RECREATE and lose the
    // first one, and its ntuple layout is frozen by the first (TODO/09 R2).
    G4Exception("RunAction::BeginOfRunAction()", "tpc0003", FatalException,
                "one /run/beamOn per macro: run the second one from its own"
                " macro, with its own /tpc settings.");
  }

  auto* analysis = G4AnalysisManager::Instance();
  const G4String currentName = analysis->GetFileName();
  if (currentName.empty()) {
    // The ".root" is spelled out: the generated name contains dots (e.g.
    // "Ar_1013.25mbar_5.5MeV") and G4AnalysisManager would take the text after
    // the last dot as the file extension.
    analysis->SetFileName(
        OutputFileName(fConfig.gas, fConfig.pressureMbar, EnergyTag()) +
        ".root");
  } else if (!EndsWithRoot(currentName)) {
    // /analysis/setFileName was given a name without ".root" (possibly with
    // dots of its own); re-set it so the file actually gets written there.
    analysis->SetFileName(currentName + ".root");
  }
  CreateNtuples();
  fNtuplesCreated = true;
  analysis->OpenFile();
  WriteRunNtuple();
}

void RunAction::EndOfRunAction(const G4Run*) {
  auto* analysis = G4AnalysisManager::Instance();
  analysis->Write();
  analysis->CloseFile();
}
