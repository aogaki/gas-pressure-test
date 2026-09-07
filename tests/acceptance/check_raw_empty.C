#include "../../include/AgetResponse.hh"
#include "AtCheck.h"

// AT4-1: a Stage 3 file whose time window caught no electron at all (the
// output of AT3-4) still gets one entry per event, with pedestal and noise
// and nothing else.
void check_raw_empty(const char* rawFile, int nEvents, double pedestal,
                     double noiseSigma, double maxExcursion) {
  TFile* file = AtOpen(rawFile);
  TTree* raw = AtTree(file, "raw");
  AtCheck(raw->GetEntries() == nEvents,
          Form("raw has %lld entries (expected %d)", raw->GetEntries(),
               nEvents));

  static Short_t adc[4][68][512];
  raw->SetBranchAddress("adc", adc);

  Long64_t outOfRange = 0, loud = 0, cells = 0;
  double sum = 0., sum2 = 0.;
  int highest = 0, lowest = kAdcMax;
  for (Long64_t i = 0; i < raw->GetEntries(); ++i) {
    raw->GetEntry(i);
    for (int a = 0; a < 4; ++a) {
      for (int c = 0; c < 68; ++c) {
        for (int t = 0; t < 512; ++t) {
          const int value = adc[a][c][t];
          if (value < 0 || value > kAdcMax) ++outOfRange;
          if (std::fabs(value - pedestal) > maxExcursion) ++loud;
          if (value > highest) highest = value;
          if (value < lowest) lowest = value;
          sum += value;
          sum2 += static_cast<double>(value) * value;
          ++cells;
        }
      }
    }
  }
  const double mean = sum / cells;
  const double rms = std::sqrt(std::max(sum2 / cells - mean * mean, 0.));
  AtCheck(outOfRange == 0,
          Form("every ADC value is 0..4095 (%lld are not)", outOfRange));
  AtCheck(loud == 0,
          Form("no cell is more than %.0f counts off the pedestal (%lld are,"
               " range %d .. %d)", maxExcursion, loud, lowest, highest));
  AtCheckNear(mean, pedestal, 0.1, "pedestal of all cells");
  AtCheckNear(rms, noiseSigma, 0.05 * noiseSigma, "noise sigma of all cells");
  AtReport();
}
