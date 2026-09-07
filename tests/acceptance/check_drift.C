#include <vector>

#include "AtCheck.h"

// AT2-3: end to end. The electrons of one Stage 1 run drift to the readout
// plane at y = -100 mm; their number and their arrival time follow the gas
// table of the run ntuple.
//
// Electrons that leave through another face of the gas volume are kept in the
// ntuple with their status, but only those on the readout plane are counted
// for the arrival time. The alpha starts 1 mm away from the z = -250 mm face,
// so a few percent of the electrons of the first hits diffuse into it.
//
// expW and expFano, when positive, are the Magboltz W [eV] and Fano factor
// expected of the gas (AT-M3); the tolerances are relative.
void check_drift(const char* fileName, const char* gas, double pressureMbar,
                 double voltage, int nEvents, double minReached,
                 double expW = 0., double expWTolPercent = 0.,
                 double expFano = 0., double expFanoTolPercent = 0.) {
  TFile* file = AtOpen(fileName);

  TTree* run = AtTree(file, "run");
  AtCheck(run->GetEntries() == 1,
          Form("run has %lld entries (expected 1)", run->GetEntries()));
  char runGas[64] = {0};
  double runPressure = 0., runVoltage = 0., runField = 0.;
  double vdrift = 0., dl = 0., dt = 0., w = 0., fano = 0.;
  run->SetBranchAddress("gas", runGas);
  run->SetBranchAddress("pressure", &runPressure);
  run->SetBranchAddress("voltage", &runVoltage);
  run->SetBranchAddress("efield", &runField);
  run->SetBranchAddress("vdrift", &vdrift);
  run->SetBranchAddress("dl", &dl);
  run->SetBranchAddress("dt", &dt);
  run->SetBranchAddress("w", &w);
  run->SetBranchAddress("fano", &fano);
  run->GetEntry(0);
  AtCheck(TString(runGas) == TString(gas),
          Form("run.gas = '%s' (expected '%s')", runGas, gas));
  AtCheckNear(runPressure, pressureMbar, 1e-9, "run.pressure [mbar]");
  AtCheckNear(runVoltage, voltage, 1e-9, "run.voltage [V]");
  AtCheckNear(runField, voltage / 20., 1e-9, "run.efield [V/cm]");
  AtCheck(vdrift > 0. && dt > 0. && w > 0. && fano > 0.,
          Form("run transport parameters: vdrift = %g cm/us, dl = %g, dt = %g,"
               " W = %g eV, Fano = %g", vdrift, dl, dt, w, fano));
  if (expW > 0.) {
    AtCheckNear(w, expW, expW * expWTolPercent / 100., "run.w [eV]");
  }
  if (expFano > 0.) {
    AtCheckNear(fano, expFano, expFano * expFanoTolPercent / 100.,
                "run.fano");
  }

  // The events of Stage 1 have been copied over.
  TTree* events = AtTree(file, "events");
  AtCheck(events->GetEntries() == nEvents,
          Form("events has %lld entries (expected %d)", events->GetEntries(),
               nEvents));
  int eventEventID = 0;
  double e0 = 0.;
  events->SetBranchAddress("eventID", &eventEventID);
  events->SetBranchAddress("e0", &e0);

  TTree* electrons = AtTree(file, "electrons");
  int eventID = 0;
  double y0 = 0., t0 = 0., x = 0., y = 0., z = 0., t = 0., weight = 0.;
  electrons->SetBranchAddress("eventID", &eventID);
  electrons->SetBranchAddress("y0", &y0);
  electrons->SetBranchAddress("t0", &t0);
  electrons->SetBranchAddress("x", &x);
  electrons->SetBranchAddress("y", &y);
  electrons->SetBranchAddress("z", &z);
  electrons->SetBranchAddress("t", &t);
  electrons->SetBranchAddress("weight", &weight);

  const Long64_t n = electrons->GetEntries();
  AtCheck(n > 0, Form("electrons has %lld entries", n));

  std::vector<double> weightSum(nEvents, 0.);
  Long64_t reached = 0, lost = 0, stuck = 0;
  double sumDrift = 0., sumExpected = 0.;
  for (Long64_t i = 0; i < n; ++i) {
    electrons->GetEntry(i);
    if (eventID >= 0 && eventID < nEvents) weightSum[eventID] += weight;
    if (std::fabs(y + 100.) <= 0.5) {
      ++reached;
      sumDrift += t - t0;
      // Drift distance / drift velocity, mm and cm/us to ns.
      sumExpected += (y0 + 100.) * 100. / vdrift;
    } else if (std::fabs(x) >= 99.5 || std::fabs(z) >= 249.5 ||
               y >= 99.5) {
      ++lost;  // left through another face of the gas volume
    } else {
      ++stuck;
    }
  }

  AtCheck(stuck == 0,
          Form("every electron ended on a face of the gas volume (%lld did"
               " not)", stuck));
  AtCheckRange(static_cast<double>(reached) / n, minReached, 1.,
               Form("fraction on the readout plane (%lld of %lld, %lld lost"
                    " sideways)", reached, n, lost));

  const double meanDrift = reached > 0 ? sumDrift / reached : 0.;
  const double meanExpected = reached > 0 ? sumExpected / reached : 0.;
  AtCheckNear(meanDrift, meanExpected, 0.05 * meanExpected,
              "mean (t - t0) [ns]");

  for (int i = 0; i < nEvents; ++i) {
    events->GetEntry(i);
    const double expected = e0 * 1e6 / w;
    AtCheckNear(weightSum[eventEventID], expected, 0.05 * expected,
                Form("event %d: sum of weight", eventEventID));
  }
  AtReport();
}
