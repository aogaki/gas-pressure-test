#include "AtCheck.h"

// AT3-4: a time window that opens after the last electron has arrived leaves
// no waveform row at all and an empty inWindow in every summary row.
void check_readout_empty(const char* readoutFile, int nEvents,
                         double windowStartUs) {
  TFile* file = AtOpen(readoutFile);

  TTree* run = AtTree(file, "run");
  double runWindow = 0.;
  run->SetBranchAddress("windowStartUs", &runWindow);
  run->GetEntry(0);
  AtCheckNear(runWindow, windowStartUs, 1e-9, "run.windowStartUs [us]");

  TTree* waveforms = AtTree(file, "waveforms");
  AtCheck(waveforms->GetEntries() == 0,
          Form("waveforms has %lld rows (expected 0)",
               waveforms->GetEntries()));

  TTree* summary = AtTree(file, "summary");
  double total = 0., onPads = 0., inWindow = 0.;
  summary->SetBranchAddress("total", &total);
  summary->SetBranchAddress("onPads", &onPads);
  summary->SetBranchAddress("inWindow", &inWindow);
  AtCheck(summary->GetEntries() == nEvents,
          Form("summary has %lld rows (expected %d)", summary->GetEntries(),
               nEvents));

  Long64_t nonEmpty = 0, noPads = 0;
  for (Long64_t i = 0; i < summary->GetEntries(); ++i) {
    summary->GetEntry(i);
    if (inWindow != 0.) ++nonEmpty;
    if (!(onPads > 0.)) ++noPads;  // the electrons still land on the pads
  }
  AtCheck(nonEmpty == 0,
          Form("every summary.inWindow is 0 (%lld are not)", nonEmpty));
  AtCheck(noPads == 0,
          Form("every event still has electrons on the pads (%lld does not)",
               noPads));
  AtReport();
}
