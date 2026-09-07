#include "AtCheck.h"

// AT-2, AT-3, AT-4: mean path length and mean projected range against the
// ASTAR expectation. checkStraggling also requires a visible range straggling.
void check_range(const char* fileName, double expTrack, double trackTolPercent,
                 double expProj, double projTolPercent, bool checkStraggling) {
  TFile* file = AtOpen(fileName);
  TTree* events = AtTree(file, "events");

  double trackLength = 0., zEnd = 0., z0 = 0.;
  int exited = 0;
  events->SetBranchAddress("trackLength", &trackLength);
  events->SetBranchAddress("zEnd", &zEnd);
  events->SetBranchAddress("z0", &z0);
  events->SetBranchAddress("exited", &exited);

  const Long64_t n = events->GetEntries();
  AtCheck(n > 0, Form("events has %lld entries", n));
  int exitedCount = 0;
  double sumTrack = 0., sumTrack2 = 0., sumProj = 0.;
  for (Long64_t i = 0; i < n; ++i) {
    events->GetEntry(i);
    if (exited != 0) ++exitedCount;
    sumTrack += trackLength;
    sumTrack2 += trackLength * trackLength;
    sumProj += zEnd - z0;
  }
  AtCheck(exitedCount == 0, Form("all alphas stop in the gas (%d exited)", exitedCount));

  const double meanTrack = sumTrack / n;
  const double meanProj = sumProj / n;
  const double sigmaTrack = std::sqrt(sumTrack2 / n - meanTrack * meanTrack);
  AtCheckNear(meanTrack, expTrack, expTrack * trackTolPercent / 100.,
              "mean trackLength [mm]");
  AtCheckNear(meanProj, expProj, expProj * projTolPercent / 100.,
              "mean projected range zEnd - z0 [mm]");
  if (checkStraggling) {
    AtCheckRange(sigmaTrack / meanTrack, 0.005, 0.03, "trackLength sigma / mean");
  }
  AtReport();
}
