#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

// The AGET response of Stage 4 (aget-shaper): the shaping function, its
// sampled kernel, the discrete convolution and the ADC conversion (TODO/07).
// Pure code, free of any ROOT, Geant4 and Garfield++ dependency.
//
// The implementation is inline so that the acceptance ROOT macros can redo
// the shaping with the same code as the application, the way PadMap.hh does
// for the pad lookup of Stage 3.

// The shaping function of the GET analysis is h(t) = (t/tau)^3 exp(-3 t/tau)
// sin(t/tau), normalised to 1 at its peak. The peak sits at t = 1.1664 tau
// (the root of 3 sin x (1 - x) + x cos x, to seven digits), so a peaking time
// t_p means tau = t_p / 1.1664.
constexpr double kAgetPeakX = 1.1664;

// The kernel is cut off after this many time constants: h(5 tau) = -8.3e-4
// (on the undershoot side), so only that tail is left.
constexpr double kAgetCutoffTau = 5.;

// The shape of the GRAW data of graw2root.C: 4 AGETs of 68 channels, each
// sampled into 512 time cells.
constexpr int kNAget = 4;
constexpr int kNChannel = 68;
constexpr int kNCells = 512;

// Elementary charge [fC]: one electron of gain-multiplied charge.
constexpr double kElectronChargeFC = 1.602e-4;

// The ADC is 12 bit, so the gain range (the full scale in fC) covers 4096
// counts.
constexpr double kAdcCounts = 4096.;
constexpr int kAdcMax = 4095;

// The four fixed-pattern-noise channels of an AGET carry no signal.
inline bool IsFpnChannel(int chGraw) {
  return chGraw == 11 || chGraw == 22 || chGraw == 45 || chGraw == 56;
}

// Time constant [ns] of a peaking time [ns].
inline double AgetTau(double peakingNs) { return peakingNs / kAgetPeakX; }

namespace aget_detail {

inline double Shape(double x) {
  return x * x * x * std::exp(-3. * x) * std::sin(x);
}

}  // namespace aget_detail

// h(t) of a peaking time [ns], 1 at the peak and 0 for t < 0.
inline double AgetResponse(double tNs, double peakingNs) {
  if (tNs < 0.) return 0.;
  return aget_detail::Shape(tNs / AgetTau(peakingNs)) /
         aget_detail::Shape(kAgetPeakX);
}

// h sampled every binNs, from t = 0 up to the cutoff at 5 tau.
inline std::vector<double> AgetKernel(double peakingNs, double binNs) {
  const std::size_t n = static_cast<std::size_t>(
      kAgetCutoffTau * AgetTau(peakingNs) / binNs) + 1;
  std::vector<double> kernel(n);
  for (std::size_t k = 0; k < n; ++k) {
    kernel[k] = AgetResponse(k * binNs, peakingNs);
  }
  return kernel;
}

// v[k] = sum_j charge[j] kernel[k - j], k - j >= 0 (the response is causal).
// The result has the length of 'charge': what the shaping pushes past the
// last time cell is outside the event window.
inline std::vector<double> AgetConvolve(const std::vector<double>& charge,
                                        const std::vector<double>& kernel) {
  std::vector<double> out(charge.size(), 0.);
  for (std::size_t j = 0; j < charge.size(); ++j) {
    if (charge[j] == 0.) continue;
    const std::size_t last = std::min(charge.size(), j + kernel.size());
    for (std::size_t k = j; k < last; ++k) {
      out[k] += charge[j] * kernel[k - j];
    }
  }
  return out;
}

// Charge [fC] of n electrons that have been multiplied by the effective gain.
inline double AgetChargeFC(double electrons, double gain) {
  return electrons * gain * kElectronChargeFC;
}

// ADC counts of a charge [fC] on a gain range [fC], before the pedestal.
inline double AgetAdcOfCharge(double chargeFC, double rangeFC) {
  return chargeFC * kAdcCounts / rangeFC;
}
