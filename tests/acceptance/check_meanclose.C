#include "AtCheck.h"

namespace {

double MeanColumn(TTree* tree, const char* column) {
  double value = 0.;
  tree->SetBranchAddress(column, &value);
  double sum = 0.;
  const Long64_t n = tree->GetEntries();
  for (Long64_t i = 0; i < n; ++i) {
    tree->GetEntry(i);
    sum += value;
  }
  return (n > 0) ? sum / n : 0.;
}

}  // namespace

// AT-10: switching the hits ntuple (and the 1 mm step limit) on must not
// change the physics.
void check_meanclose(const char* fileA, const char* fileB, double tolPercent) {
  TFile* a = AtOpen(fileA);
  TFile* b = AtOpen(fileB);
  const double meanA = MeanColumn(AtTree(a, "events"), "trackLength");
  const double meanB = MeanColumn(AtTree(b, "events"), "trackLength");
  std::printf("       mean trackLength: %.6g mm and %.6g mm\n", meanA, meanB);
  AtCheckNear(meanB, meanA, meanA * tolPercent / 100.,
              "mean trackLength with hits on [mm]");
  AtReport();
}
