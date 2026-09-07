// AT2-2: 1000 electrons drift from the centre of the TPC (0, 0, 0) down to the
// readout plane at y = -100 mm. Arrival time and transverse spread must follow
// the drift velocity and the transverse diffusion of the gas table.
//
// Takes the gas table cache directory as its only argument.

#include <cmath>
#include <cstdio>
#include <string>

#include "AtCheck.h"
#include "Drifter.hh"

int main(int argc, char** argv) {
  const std::string cacheDir = argc > 1 ? argv[1] : "gasfiles";
  const double pressureMbar = 200.;
  const double fieldVcm = 100.;
  const double driftMm = 100.;
  const int nElectrons = 1000;

  Drifter drifter("Ar", pressureMbar, fieldVcm, cacheDir);
  std::printf("[ok]   Ar at %g mbar, %g V/cm: vdrift = %g cm/us, dl = %g,"
              " dt = %g sqrt(cm)\n",
              pressureMbar, fieldVcm, drifter.DriftVelocity(), drifter.Dl(),
              drifter.Dt());

  int reached = 0, failed = 0;
  double sumT = 0., sumX = 0., sumX2 = 0.;
  for (int i = 0; i < nElectrons; ++i) {
    DriftEndpoint end;
    if (!drifter.Drift(0., 0., 0., 0., end)) {
      ++failed;
      continue;
    }
    if (std::fabs(end.y + driftMm) <= 1.) ++reached;
    sumT += end.t;
    sumX += end.x;
    sumX2 += end.x * end.x;
  }
  AtCheck(failed == 0, Form("all %d electrons drifted (%d failed)", nElectrons,
                            failed));
  AtCheck(reached == nElectrons,
          Form("%d of %d electrons reached y = -100 mm (+- 1 mm)", reached,
               nElectrons));

  // Drift distance / drift velocity, mm and cm/us converted to ns.
  const double expectedT = driftMm * 100. / drifter.DriftVelocity();
  const double meanT = sumT / nElectrons;
  AtCheckNear(meanT, expectedT, 0.05 * expectedT, "mean arrival time [ns]");

  // sigma_T = D_T * sqrt(L), with D_T in sqrt(cm) and L in cm.
  const double expectedSd = 10. * drifter.Dt() * std::sqrt(0.1 * driftMm);
  const double meanX = sumX / nElectrons;
  const double sdX = std::sqrt(sumX2 / nElectrons - meanX * meanX);
  AtCheckNear(sdX, expectedSd, 0.15 * expectedSd, "sd(x) [mm]");

  AtReport();
  return 0;
}
