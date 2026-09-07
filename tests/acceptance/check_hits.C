#include "AtCheck.h"

// AT-9: every hit is inside the gas, obeys the 1 mm step limit and has edep > 0.
void check_hits(const char* fileName, double maxStep) {
  TFile* file = AtOpen(fileName);
  TTree* hits = AtTree(file, "hits");

  double x = 0., y = 0., z = 0., edep = 0., stepLength = 0.;
  hits->SetBranchAddress("x", &x);
  hits->SetBranchAddress("y", &y);
  hits->SetBranchAddress("z", &z);
  hits->SetBranchAddress("edep", &edep);
  hits->SetBranchAddress("stepLength", &stepLength);

  const Long64_t n = hits->GetEntries();
  AtCheck(n > 0, Form("hits has %lld entries", n));
  int tooLong = 0, outside = 0, noEdep = 0;
  for (Long64_t i = 0; i < n; ++i) {
    hits->GetEntry(i);
    if (stepLength > maxStep + 1e-6) ++tooLong;
    if (std::fabs(x) > 100. + 1e-6 || std::fabs(y) > 100. + 1e-6 ||
        std::fabs(z) > 250. + 1e-6) {
      ++outside;
    }
    if (!(edep > 0.)) ++noEdep;
  }
  AtCheck(tooLong == 0, Form("stepLength <= %g mm (%d violations)", maxStep, tooLong));
  AtCheck(outside == 0, Form("all hits inside the gas box (%d outside)", outside));
  AtCheck(noEdep == 0, Form("all hits have edep > 0 (%d without)", noEdep));
  AtReport();
}
