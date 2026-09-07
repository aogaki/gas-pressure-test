#include "AtCheck.h"

// AT-11: an isotropic cone around +z is recorded event by event.
void check_gps_angle(const char* fileName, double dz0Min) {
  TFile* file = AtOpen(fileName);
  TTree* events = AtTree(file, "events");

  double dx0 = 0., dz0 = 0.;
  events->SetBranchAddress("dx0", &dx0);
  events->SetBranchAddress("dz0", &dz0);
  const Long64_t n = events->GetEntries();
  AtCheck(n > 0, Form("events has %lld entries", n));
  int belowMin = 0;
  double sum = 0., sum2 = 0., lowest = 1e30;
  for (Long64_t i = 0; i < n; ++i) {
    events->GetEntry(i);
    if (dz0 < dz0Min) ++belowMin;
    if (dz0 < lowest) lowest = dz0;
    sum += dx0;
    sum2 += dx0 * dx0;
  }
  const double mean = sum / n;
  const double sigma = std::sqrt(sum2 / n - mean * mean);
  AtCheck(belowMin == 0,
          Form("dz0 >= %g for every event (min %.5f, %d below)", dz0Min, lowest,
               belowMin));
  AtCheck(sigma > 0., Form("dx0 sigma = %.4g > 0", sigma));
  AtReport();
}
