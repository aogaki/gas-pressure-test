#include "AtCheck.h"

// AT4-4: the closed loop. The "raw" file of aget-shaper has gone through
// analyzeUVW.C and makeTracks.C, the very macros that analyse the real
// detector, and the "tracks" tree has to hold the alpha that Stage 1 shot.
//
// On the length: makeTracks.C does not measure the range of the alpha. It
// puts the endpoints at the 2 % and 98 % charge quantiles of the 3 mm slice
// centroids, and both definitions cut into a track that stops in the gas.
// For the ten alphas of this test (CSDA range 72.8 mm, straight-line extent
// 70.9 mm) the 2 %/98 % quantiles of the true energy deposit already give
// 66.4 mm, and the slice centroids take up to 1.5 mm off each end of the
// drift extent, which is another 6 mm along a track inclined by 30 degrees.
// The expected length is therefore about 60 mm, not the CSDA range, and the
// measurement gives 60.6 mm (the error on the mean of ten tracks is 0.6 mm);
// a wrong drift velocity or strip pitch would still show up, in the length
// as well as in the direction cosine.
void check_tracks(const char* tracksFile, int nEvents, int minOk,
                  double expectedLengthMm, double lengthTolerance,
                  double expectedDirZ, double dirTolerance) {
  TFile* file = AtOpen(tracksFile);
  TTree* tracks = AtTree(file, "tracks");

  UInt_t eventId = 0;
  int ok = 0;
  float length = 0., charge = 0., dirX = 0., dirY = 0., dirZ = 0.;
  tracks->SetBranchAddress("eventId", &eventId);
  tracks->SetBranchAddress("ok", &ok);
  tracks->SetBranchAddress("length", &length);
  tracks->SetBranchAddress("charge", &charge);
  tracks->SetBranchAddress("dirX", &dirX);
  tracks->SetBranchAddress("dirY", &dirY);
  tracks->SetBranchAddress("dirZ", &dirZ);

  AtCheck(tracks->GetEntries() == nEvents,
          Form("tracks has %lld entries (expected %d)", tracks->GetEntries(),
               nEvents));

  int nOk = 0;
  double sumLength = 0., sumDirZ = 0., sumCharge = 0.;
  double minLength = 1e30, maxLength = -1e30;
  for (Long64_t i = 0; i < tracks->GetEntries(); ++i) {
    tracks->GetEntry(i);
    if (ok != 1) continue;
    ++nOk;
    sumLength += length;
    sumDirZ += std::fabs(dirZ);
    sumCharge += charge;
    minLength = std::min(minLength, static_cast<double>(length));
    maxLength = std::max(maxLength, static_cast<double>(length));
  }

  AtCheck(nOk >= minOk,
          Form("%d of %lld events have ok = 1 (at least %d)", nOk,
               tracks->GetEntries(), minOk));
  if (nOk == 0) AtReport();

  AtCheckNear(sumLength / nOk, expectedLengthMm,
              lengthTolerance * expectedLengthMm, "mean track length [mm]");
  // The alpha was shot at (0, 0.5, 0.866): half of its direction is along
  // the drift axis, which is the z of the analysis macros.
  AtCheckNear(sumDirZ / nOk, expectedDirZ, dirTolerance,
              "mean |direction cosine| along the drift axis");
  std::printf("[info] %d tracks: length %.2f .. %.2f mm, mean charge %.4g"
              " ADC\n", nOk, minLength, maxLength, sumCharge / nOk);
  AtReport();
}
