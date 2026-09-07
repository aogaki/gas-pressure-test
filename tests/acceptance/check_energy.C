#include "AtCheck.h"

#include <map>

// AT-6: the hits add up to edepTotal, and the alpha deposits nearly all of e0.
void check_energy(const char* fileName) {
  TFile* file = AtOpen(fileName);
  TTree* events = AtTree(file, "events");
  TTree* hits = AtTree(file, "hits");

  std::map<int, double> hitSum;
  int hitEventID = 0;
  double edep = 0.;
  hits->SetBranchAddress("eventID", &hitEventID);
  hits->SetBranchAddress("edep", &edep);
  for (Long64_t i = 0; i < hits->GetEntries(); ++i) {
    hits->GetEntry(i);
    hitSum[hitEventID] += edep;
  }

  int eventID = 0;
  double edepTotal = 0., e0 = 0.;
  events->SetBranchAddress("eventID", &eventID);
  events->SetBranchAddress("edepTotal", &edepTotal);
  events->SetBranchAddress("e0", &e0);

  const Long64_t n = events->GetEntries();
  AtCheck(n > 0, Form("events has %lld entries", n));
  int mismatched = 0;
  double worst = 0., sumEdep = 0., sumE0 = 0.;
  for (Long64_t i = 0; i < n; ++i) {
    events->GetEntry(i);
    const double difference = std::fabs(hitSum[eventID] - edepTotal);
    if (difference > 1e-6) ++mismatched;
    if (difference > worst) worst = difference;
    sumEdep += edepTotal;
    sumE0 += e0;
  }
  AtCheck(mismatched == 0,
          Form("sum(hits.edep) == events.edepTotal for every event "
               "(%d mismatches, worst %.3g MeV)", mismatched, worst));
  // The upper bound carries 1e-9 of slack: the ratio is exactly 1 when no
  // alpha leaves the gas, and rounding in the sums can push it a few ulp over.
  AtCheckRange(sumEdep / sumE0, 0.999, 1.0 + 1e-9, "mean edepTotal / mean e0");
  AtReport();
}
