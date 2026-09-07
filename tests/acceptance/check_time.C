#include "AtCheck.h"

// AT-8: the primary hit times increase along the track, and tEnd is sensible.
void check_time(const char* fileName, double tEndMin, double tEndMax) {
  TFile* file = AtOpen(fileName);
  TTree* events = AtTree(file, "events");
  TTree* hits = AtTree(file, "hits");

  int hitEventID = 0, trackID = 0;
  double t = 0.;
  hits->SetBranchAddress("eventID", &hitEventID);
  hits->SetBranchAddress("trackID", &trackID);
  hits->SetBranchAddress("t", &t);

  int decreasing = 0;
  int previousEvent = -1;
  double previousTime = 0.;
  for (Long64_t i = 0; i < hits->GetEntries(); ++i) {
    hits->GetEntry(i);
    if (trackID != 1) continue;
    if (hitEventID != previousEvent) {
      previousEvent = hitEventID;
    } else if (t < previousTime) {
      ++decreasing;
    }
    previousTime = t;
  }
  AtCheck(decreasing == 0,
          Form("primary hit times are non decreasing (%d steps back in time)",
               decreasing));

  double tEnd = 0.;
  events->SetBranchAddress("tEnd", &tEnd);
  const Long64_t n = events->GetEntries();
  double sum = 0.;
  for (Long64_t i = 0; i < n; ++i) {
    events->GetEntry(i);
    sum += tEnd;
  }
  AtCheckRange(sum / n, tEndMin, tEndMax, "mean tEnd [ns]");
  AtReport();
}
