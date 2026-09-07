#include "AtCheck.h"

// AT-11: a flat GPS spectrum is recorded event by event.
void check_gps_energy(const char* fileName, double eMin, double eMax,
                      double sigmaMin) {
  TFile* file = AtOpen(fileName);
  TTree* events = AtTree(file, "events");

  double e0 = 0.;
  events->SetBranchAddress("e0", &e0);
  const Long64_t n = events->GetEntries();
  AtCheck(n > 0, Form("events has %lld entries", n));
  double lowest = 1e30, highest = -1e30, sum = 0., sum2 = 0.;
  for (Long64_t i = 0; i < n; ++i) {
    events->GetEntry(i);
    if (e0 < lowest) lowest = e0;
    if (e0 > highest) highest = e0;
    sum += e0;
    sum2 += e0 * e0;
  }
  const double mean = sum / n;
  const double sigma = std::sqrt(sum2 / n - mean * mean);
  AtCheck(lowest >= eMin, Form("min e0 = %.4g MeV >= %g MeV", lowest, eMin));
  AtCheck(highest <= eMax, Form("max e0 = %.4g MeV <= %g MeV", highest, eMax));
  AtCheck(sigma >= sigmaMin,
          Form("e0 sigma = %.4g MeV >= %g MeV", sigma, sigmaMin));
  AtReport();
}
