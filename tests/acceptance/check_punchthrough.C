#include "AtCheck.h"

// AT-5: every alpha leaves the gas through the downstream face.
void check_punchthrough(const char* fileName, double trackMin, double trackMax,
                        double zEndExpected, double zEndTolerance,
                        double eExitExpected, double eExitTolPercent) {
  TFile* file = AtOpen(fileName);
  TTree* events = AtTree(file, "events");

  double trackLength = 0., zEnd = 0., eExit = 0.;
  int exited = 0;
  events->SetBranchAddress("trackLength", &trackLength);
  events->SetBranchAddress("zEnd", &zEnd);
  events->SetBranchAddress("eExit", &eExit);
  events->SetBranchAddress("exited", &exited);

  const Long64_t n = events->GetEntries();
  AtCheck(n > 0, Form("events has %lld entries", n));
  int notExited = 0, badTrack = 0, badZ = 0;
  double sumEExit = 0.;
  for (Long64_t i = 0; i < n; ++i) {
    events->GetEntry(i);
    if (exited != 1) ++notExited;
    if (trackLength < trackMin || trackLength > trackMax) ++badTrack;
    if (std::fabs(zEnd - zEndExpected) > zEndTolerance) ++badZ;
    sumEExit += eExit;
  }
  AtCheck(notExited == 0, Form("all alphas exit (%d did not)", notExited));
  AtCheck(badTrack == 0, Form("trackLength within [%g, %g] mm (%d outside)",
                              trackMin, trackMax, badTrack));
  AtCheck(badZ == 0, Form("zEnd = %g mm within %g mm (%d outside)",
                          zEndExpected, zEndTolerance, badZ));
  AtCheckNear(sumEExit / n, eExitExpected,
              eExitExpected * eExitTolPercent / 100., "mean eExit [MeV]");
  AtReport();
}
