#include "AtCheck.h"

// AT-S2: one scan.sh run produced the expected number of events at the
// expected pressure. Unlike check_events.C this does not assume the
// default 5.5 MeV primary energy, since scan.sh always sets /gps/energy.
void check_scan_run(const char* fileName, int nEvents, double pressureMbar) {
  TFile* file = AtOpen(fileName);
  TTree* events = AtTree(file, "events");
  AtCheck(events->GetEntries() == nEvents,
          Form("events has %lld entries (expected %d)", events->GetEntries(), nEvents));

  TTree* run = AtTree(file, "run");
  double runPressure = 0.;
  run->SetBranchAddress("pressure", &runPressure);
  run->GetEntry(0);
  AtCheckNear(runPressure, pressureMbar, 1e-9, "run.pressure [mbar]");
  AtReport();
}
