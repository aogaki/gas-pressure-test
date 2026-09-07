#include <cstdio>
#include <cstring>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>

#include "Drifter.hh"
#include "DriftUtils.hh"
#include "TFile.h"
#include "TTree.h"

namespace {

constexpr unsigned int kRandomSeed = 12345;

// One row of the "electrons" ntuple of TODO/03.
struct ElectronRow {
  int eventID = 0;
  double x0 = 0., y0 = 0., z0 = 0., t0 = 0.;
  double x = 0., y = 0., z = 0., t = 0.;
  double weight = 1.;
  int status = 0;
};

}  // namespace

int main(int argc, char** argv) {
  DriftOptions options;
  std::string error;
  if (!ParseDriftOptions(argc, argv, options, error)) {
    std::fprintf(stderr, "drift-electrons: %s\n%s\n", error.c_str(),
                 DriftUsage().c_str());
    return 1;
  }
  if (options.help) {
    std::printf("%s\n", DriftUsage().c_str());
    return 0;
  }

  TFile input(options.input.c_str());
  if (input.IsZombie()) {
    std::fprintf(stderr, "drift-electrons: cannot open %s\n",
                 options.input.c_str());
    return 1;
  }

  // The run conditions of Stage 1 (TODO/01): gas and pressure.
  TTree* runIn = input.Get<TTree>("run");
  if (runIn == nullptr || runIn->GetEntries() < 1) {
    std::fprintf(stderr, "drift-electrons: no 'run' ntuple in %s\n",
                 options.input.c_str());
    return 1;
  }
  char gas[64] = {0};
  double pressureMbar = 0.;
  runIn->SetBranchAddress("gas", gas);
  runIn->SetBranchAddress("pressure", &pressureMbar);
  runIn->GetEntry(0);

  TTree* hits = input.Get<TTree>("hits");
  if (hits == nullptr) {
    std::fprintf(stderr,
                 "drift-electrons: no 'hits' ntuple in %s, rerun Stage 1 with"
                 " /tpc/hits true\n",
                 options.input.c_str());
    return 1;
  }
  int hitEventID = 0;
  double hitX = 0., hitY = 0., hitZ = 0., hitT = 0., hitEdep = 0.;
  hits->SetBranchAddress("eventID", &hitEventID);
  hits->SetBranchAddress("x", &hitX);
  hits->SetBranchAddress("y", &hitY);
  hits->SetBranchAddress("z", &hitZ);
  hits->SetBranchAddress("t", &hitT);
  hits->SetBranchAddress("edep", &hitEdep);

  const double fieldVcm = DriftField(options.voltage);
  std::unique_ptr<Drifter> owned;
  try {
    owned.reset(new Drifter(gas, pressureMbar, fieldVcm, options.cacheDir));
  } catch (const std::exception& e) {
    std::fprintf(stderr, "drift-electrons: %s\n", e.what());
    return 1;
  }
  Drifter& drifter = *owned;
  std::printf(
      "drift-electrons: %s at %g mbar, %g V (%g V/cm): vdrift = %g cm/us,"
      " dl = %g, dt = %g sqrt(cm), W = %g eV, Fano = %g\n",
      gas, pressureMbar, options.voltage, fieldVcm, drifter.DriftVelocity(),
      drifter.Dl(), drifter.Dt(), drifter.W(), drifter.Fano());

  const std::string outputName =
      DriftOutputName(options.input, options.voltage);
  TFile output(outputName.c_str(), "RECREATE");
  if (output.IsZombie()) {
    std::fprintf(stderr, "drift-electrons: cannot write %s\n",
                 outputName.c_str());
    return 1;
  }

  // The events of Stage 1 travel along unchanged, for the analysis.
  TTree* eventsIn = input.Get<TTree>("events");
  if (eventsIn != nullptr) {
    output.cd();
    eventsIn->CloneTree(-1, "fast");
  }

  output.cd();
  ElectronRow row;
  TTree* electrons = new TTree("electrons", "One row per drifted electron");
  electrons->Branch("eventID", &row.eventID, "eventID/I");
  electrons->Branch("x0", &row.x0, "x0/D");
  electrons->Branch("y0", &row.y0, "y0/D");
  electrons->Branch("z0", &row.z0, "z0/D");
  electrons->Branch("t0", &row.t0, "t0/D");
  electrons->Branch("x", &row.x, "x/D");
  electrons->Branch("y", &row.y, "y/D");
  electrons->Branch("z", &row.z, "z/D");
  electrons->Branch("t", &row.t, "t/D");
  electrons->Branch("weight", &row.weight, "weight/D");
  electrons->Branch("status", &row.status, "status/I");

  std::mt19937 rng(kRandomSeed);
  const double weight = 1. / options.fraction;
  long long created = 0, drifted = 0, failed = 0;
  const Long64_t nHits = hits->GetEntries();
  for (Long64_t i = 0; i < nHits; ++i) {
    hits->GetEntry(i);
    if (options.maxEvents > 0 && hitEventID >= options.maxEvents) continue;
    const int n =
        SampleElectronCount(hitEdep, drifter.W(), drifter.Fano(), rng);
    created += n;
    const int nDrift = SampleDriftedCount(n, options.fraction, rng);
    for (int k = 0; k < nDrift; ++k) {
      DriftEndpoint end;
      if (!drifter.Drift(hitX, hitY, hitZ, hitT, end)) {
        ++failed;
        continue;
      }
      row.eventID = hitEventID;
      row.x0 = hitX;
      row.y0 = hitY;
      row.z0 = hitZ;
      row.t0 = hitT;
      row.x = end.x;
      row.y = end.y;
      row.z = end.z;
      row.t = end.t;
      row.weight = weight;
      row.status = end.status;
      electrons->Fill();
      ++drifted;
    }
  }

  TTree* runOut = new TTree("run", "One row with the drift conditions");
  char outGas[64] = {0};
  std::strncpy(outGas, gas, sizeof(outGas) - 1);
  double voltage = options.voltage;
  double efield = fieldVcm;
  double vdrift = drifter.DriftVelocity();
  double dl = drifter.Dl();
  double dt = drifter.Dt();
  double w = drifter.W();
  double fano = drifter.Fano();
  runOut->Branch("gas", outGas, "gas/C");
  runOut->Branch("pressure", &pressureMbar, "pressure/D");
  runOut->Branch("voltage", &voltage, "voltage/D");
  runOut->Branch("efield", &efield, "efield/D");
  runOut->Branch("vdrift", &vdrift, "vdrift/D");
  runOut->Branch("dl", &dl, "dl/D");
  runOut->Branch("dt", &dt, "dt/D");
  runOut->Branch("w", &w, "w/D");
  runOut->Branch("fano", &fano, "fano/D");
  runOut->Fill();

  output.Write();
  output.Close();

  std::printf(
      "drift-electrons: %lld electrons created, %lld drifted (%lld failed)"
      " -> %s\n",
      created, drifted, failed, outputName.c_str());
  return 0;
}
