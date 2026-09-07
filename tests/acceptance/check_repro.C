#include "AtCheck.h"

#include <vector>

namespace {

const char* kIntColumns[] = {"eventID", "exited"};
const char* kDoubleColumns[] = {"e0",   "x0",   "y0",    "z0",
                                "dx0",  "dy0",  "dz0",   "trackLength",
                                "xEnd", "yEnd", "zEnd",  "tEnd",
                                "eExit", "edepTotal"};

}  // namespace

// AT-7: two runs with the same seeds must agree bit for bit, and a run with
// different seeds must differ.
void check_repro(const char* fileA, const char* fileB, bool expectIdentical) {
  TFile* a = AtOpen(fileA);
  TFile* b = AtOpen(fileB);
  TTree* eventsA = AtTree(a, "events");
  TTree* eventsB = AtTree(b, "events");

  const Long64_t n = eventsA->GetEntries();
  AtCheck(n == eventsB->GetEntries(),
          Form("same number of events (%lld vs %lld)", n, eventsB->GetEntries()));

  const int nInt = sizeof(kIntColumns) / sizeof(kIntColumns[0]);
  const int nDouble = sizeof(kDoubleColumns) / sizeof(kDoubleColumns[0]);
  std::vector<int> intA(nInt), intB(nInt);
  std::vector<double> doubleA(nDouble), doubleB(nDouble);
  for (int c = 0; c < nInt; ++c) {
    eventsA->SetBranchAddress(kIntColumns[c], &intA[c]);
    eventsB->SetBranchAddress(kIntColumns[c], &intB[c]);
  }
  for (int c = 0; c < nDouble; ++c) {
    eventsA->SetBranchAddress(kDoubleColumns[c], &doubleA[c]);
    eventsB->SetBranchAddress(kDoubleColumns[c], &doubleB[c]);
  }

  int differing = 0;
  for (Long64_t i = 0; i < n; ++i) {
    eventsA->GetEntry(i);
    eventsB->GetEntry(i);
    bool same = true;
    for (int c = 0; c < nInt; ++c) {
      if (intA[c] != intB[c]) same = false;
    }
    for (int c = 0; c < nDouble; ++c) {
      if (doubleA[c] != doubleB[c]) same = false;
    }
    if (!same) ++differing;
  }

  if (expectIdentical) {
    AtCheck(differing == 0,
            Form("all columns identical in every event (%d differ)", differing));
  } else {
    AtCheck(differing > 0,
            Form("different seeds give different events (%d of %lld differ)",
                 differing, n));
  }
  AtReport();
}
