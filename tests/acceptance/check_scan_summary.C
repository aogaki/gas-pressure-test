#include "AtCheck.h"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {
const char* kHeader =
    "gas,pressure_mbar,energy_MeV,events,mean_trackLength_mm,"
    "sigma_trackLength_mm,mean_projected_mm,exited_fraction,mean_eExit_MeV,"
    "contained_fraction";
}  // namespace

// AT-S3: range_summary.C's CSV for two energies of the same gas/pressure.
// csvFileName is the captured stdout of `root -l -b -q range_summary.C(...)`,
// which also contains ROOT's own "Processing ..." banner, so the header is
// found by content rather than assumed to be the first line.
//
// contained0 and contained1 are the expected contained_fraction of the two
// rows. They are 1 for the default detection area (the whole gas volume) and
// tell the two energies apart once the area is cut down to the readout board.
void check_scan_summary(const char* csvFileName, double contained0 = 1.,
                        double contained1 = 1.) {
  std::ifstream in(csvFileName);
  AtCheck(static_cast<bool>(in), Form("opened %s", csvFileName));
  if (!in) {
    AtReport();
    return;
  }

  std::string line;
  bool sawHeader = false;
  std::vector<std::string> rows;
  while (std::getline(in, line)) {
    if (line == kHeader) {
      sawHeader = true;
      continue;
    }
    if (sawHeader && !line.empty()) rows.push_back(line);
  }
  AtCheck(sawHeader, "CSV header matches the documented columns");
  AtCheck(rows.size() == 2, Form("CSV has %zu data row(s) (expected 2)", rows.size()));

  const double expectedEnergy[2] = {0.3, 1.0};
  const double expectedContained[2] = {contained0, contained1};
  for (size_t i = 0; i < rows.size() && i < 2; ++i) {
    std::stringstream ss(rows[i]);
    std::vector<std::string> fields;
    std::string field;
    while (std::getline(ss, field, ',')) fields.push_back(field);
    AtCheck(fields.size() == 10,
            Form("row %zu has %zu columns (expected 10)", i, fields.size()));
    if (fields.size() != 10) continue;
    AtCheckNear(std::atof(fields[2].c_str()), expectedEnergy[i], 1e-6,
                Form("row %zu energy_MeV", i));
    AtCheck(std::atoi(fields[3].c_str()) == 10, Form("row %zu events == 10", i));
    AtCheck(std::atof(fields[4].c_str()) > 0.,
            Form("row %zu mean_trackLength_mm > 0", i));
    AtCheckNear(std::atof(fields[9].c_str()), expectedContained[i], 1e-9,
                Form("row %zu contained_fraction", i));
  }
  AtReport();
}
