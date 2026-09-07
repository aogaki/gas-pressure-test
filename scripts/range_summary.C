// Summarises a directory of Stage 1 ROOT files into one CSV row per file.
// See TODO/04_scan.md.
//
// Usage: root -l -b -q 'scripts/range_summary.C("<outdir>")'
//
// NOTE: mean_trackLength_mm and mean_projected_mm include punch-through
// events (exited == 1). For a row with exited_fraction > 0 those means are
// therefore biased low: the path length is cut off by the gas volume, not
// the alpha's true range. Check exited_fraction before trusting the range.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "TFile.h"
#include "TString.h"
#include "TSystem.h"
#include "TTree.h"

namespace {

struct Row {
  std::string gas;
  double pressure = 0.;
  double energy = 0.;
  Long64_t events = 0;
  double meanTrack = 0.;
  double sigmaTrack = 0.;
  double meanProj = 0.;
  double exitedFraction = 0.;
  double meanEExit = 0.;
};

// Reads one Stage 1 ROOT file and fills row. Returns false (with a message
// on stderr) if the file cannot be used.
bool ReadFile(const char* path, Row& row) {
  TFile* file = TFile::Open(path);
  if (file == nullptr || file->IsZombie()) {
    std::fprintf(stderr, "range_summary: cannot open %s\n", path);
    return false;
  }
  TTree* run = dynamic_cast<TTree*>(file->Get("run"));
  TTree* events = dynamic_cast<TTree*>(file->Get("events"));
  if (run == nullptr || events == nullptr) {
    std::fprintf(stderr, "range_summary: %s has no run/events ntuple\n", path);
    delete file;
    return false;
  }

  char gas[64] = {0};
  double pressure = 0.;
  run->SetBranchAddress("gas", gas);
  run->SetBranchAddress("pressure", &pressure);
  run->GetEntry(0);

  double e0 = 0., z0 = 0., trackLength = 0., zEnd = 0., eExit = 0.;
  int exited = 0;
  events->SetBranchAddress("e0", &e0);
  events->SetBranchAddress("z0", &z0);
  events->SetBranchAddress("trackLength", &trackLength);
  events->SetBranchAddress("zEnd", &zEnd);
  events->SetBranchAddress("eExit", &eExit);
  events->SetBranchAddress("exited", &exited);

  const Long64_t n = events->GetEntries();
  double sumE0 = 0., sumTrack = 0., sumTrack2 = 0., sumProj = 0., sumEExit = 0.;
  Long64_t exitedCount = 0;
  for (Long64_t i = 0; i < n; ++i) {
    events->GetEntry(i);
    sumE0 += e0;
    sumTrack += trackLength;
    sumTrack2 += trackLength * trackLength;
    sumProj += zEnd - z0;
    sumEExit += eExit;
    if (exited != 0) ++exitedCount;
  }

  row.gas = gas;
  row.pressure = pressure;
  row.events = n;
  if (n > 0) {
    row.energy = sumE0 / n;
    row.meanTrack = sumTrack / n;
    const double variance = sumTrack2 / n - row.meanTrack * row.meanTrack;
    row.sigmaTrack = variance > 0. ? std::sqrt(variance) : 0.;
    row.meanProj = sumProj / n;
    row.exitedFraction = static_cast<double>(exitedCount) / n;
    row.meanEExit = sumEExit / n;
  }
  delete file;
  return true;
}

}  // namespace

void range_summary(const char* outdir) {
  std::vector<Row> rows;
  void* dirp = gSystem->OpenDirectory(outdir);
  if (dirp == nullptr) {
    std::fprintf(stderr, "range_summary: cannot open directory %s\n", outdir);
    return;
  }
  const char* entry = nullptr;
  while ((entry = gSystem->GetDirEntry(dirp)) != nullptr) {
    const TString name(entry);
    if (!name.EndsWith(".root")) continue;
    Row row;
    if (ReadFile(TString::Format("%s/%s", outdir, name.Data()), row)) {
      rows.push_back(row);
    }
  }
  gSystem->FreeDirectory(dirp);

  std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
    if (a.gas != b.gas) return a.gas < b.gas;
    if (a.pressure != b.pressure) return a.pressure < b.pressure;
    return a.energy < b.energy;
  });

  std::printf(
      "gas,pressure_mbar,energy_MeV,events,mean_trackLength_mm,"
      "sigma_trackLength_mm,mean_projected_mm,exited_fraction,"
      "mean_eExit_MeV\n");
  for (const Row& row : rows) {
    std::printf("%s,%g,%g,%lld,%g,%g,%g,%g,%g\n", row.gas.c_str(),
                row.pressure, row.energy, row.events, row.meanTrack,
                row.sigmaTrack, row.meanProj, row.exitedFraction,
                row.meanEExit);
  }
}
