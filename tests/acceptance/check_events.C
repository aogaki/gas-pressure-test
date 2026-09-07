#include "AtCheck.h"

// AT-1: entry count, presence of the hits ntuple and the primary defaults.
// hitsMode 0: the hits ntuple must be absent, 1: it must hold at least one row.
void check_events(const char* fileName, int nEvents, int hitsMode) {
  TFile* file = AtOpen(fileName);
  TTree* events = AtTree(file, "events");
  AtCheck(events->GetEntries() == nEvents, Form("events has %lld entries (expected %d)",
                                                events->GetEntries(), nEvents));

  double e0 = 0., x0 = 0., y0 = 0., z0 = 0., dz0 = 0.;
  events->SetBranchAddress("e0", &e0);
  events->SetBranchAddress("x0", &x0);
  events->SetBranchAddress("y0", &y0);
  events->SetBranchAddress("z0", &z0);
  events->SetBranchAddress("dz0", &dz0);
  events->GetEntry(0);
  AtCheckNear(e0, 5.5, 1e-9, "default e0 [MeV]");
  AtCheckNear(x0, 0., 1e-9, "default x0 [mm]");
  AtCheckNear(y0, 0., 1e-9, "default y0 [mm]");
  AtCheckNear(z0, -250., 1e-9, "default z0 [mm]");
  AtCheckNear(dz0, 1., 1e-9, "default dz0");

  TTree* hits = dynamic_cast<TTree*>(file->Get("hits"));
  if (hitsMode == 0) {
    AtCheck(hits == nullptr, "hits ntuple is absent");
  } else {
    AtCheck(hits != nullptr, "hits ntuple is present");
    if (hits != nullptr) {
      AtCheck(hits->GetEntries() >= 1, Form("hits has %lld entries", hits->GetEntries()));
    }
  }
  AtReport();
}
