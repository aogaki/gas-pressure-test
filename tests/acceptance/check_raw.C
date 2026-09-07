#include <map>
#include <utility>
#include <vector>

#include "../../include/AgetResponse.hh"
#include "AtCheck.h"

// AT4-1 (the tree) and AT4-3 (end to end): the "raw" tree of aget-shaper is
// the one graw2root.C writes for real data, its quiet cells carry pedestal
// and noise only, the FPN channels carry no signal, and the area of every
// pulse is the charge of Stage 3 times the area of the shaping.
//
// AgetResponse.hh is header-only, so this macro shapes the Stage 3 electrons
// with the same code as the application and compares the result.
//
// The pedestal window is the one of analyzeUVW.C: time cells 1 to 20.

// The electrons of one (event, channel) of the Stage 3 file.
struct RawCharge {
  double electrons = 0.;
  int firstBin = 1 << 30;
  int lastBin = -1;
};

void check_raw(const char* rawFile, const char* readoutFile, int nEvents,
               double pedestal, double noiseSigma, double gain,
               double rangeFC, double peakingNs, double minPulseAdc,
               double minAreaAdc = 1000.) {
  const int kPedLo = 1, kPedHi = 20;

  TFile* file = AtOpen(rawFile);
  TTree* raw = AtTree(file, "raw");

  // --- AT4-1: the tree of graw2root.C, column for column -------------------
  AtCheck(raw->GetEntries() == nEvents,
          Form("raw has %lld entries (expected %d)", raw->GetEntries(),
               nEvents));
  AtCheck(raw->GetBranch("eventId") != nullptr, "raw has eventId");
  AtCheck(raw->GetBranch("eventTime") != nullptr, "raw has eventTime");
  TBranch* adcBranch = raw->GetBranch("adc");
  AtCheck(adcBranch != nullptr, "raw has adc");
  if (adcBranch != nullptr) {
    AtCheck(TString(adcBranch->GetTitle()) == "adc[4][68][512]/S",
            Form("adc is '%s' (expected 'adc[4][68][512]/S')",
                 adcBranch->GetTitle()));
  }

  UInt_t eventId = 0;
  ULong64_t eventTime = 0;
  static Short_t adc[4][68][512];
  raw->SetBranchAddress("eventId", &eventId);
  raw->SetBranchAddress("eventTime", &eventTime);
  raw->SetBranchAddress("adc", adc);

  // --- the electrons of Stage 3, per event and channel ---------------------
  TFile* readout = AtOpen(readoutFile);
  TTree* waveforms = AtTree(readout, "waveforms");
  int wfEventID = 0, wfAget = 0, wfChGraw = 0, wfBin = 0;
  double wfElectrons = 0.;
  waveforms->SetBranchAddress("eventID", &wfEventID);
  waveforms->SetBranchAddress("aget", &wfAget);
  waveforms->SetBranchAddress("ch_graw", &wfChGraw);
  waveforms->SetBranchAddress("bin", &wfBin);
  waveforms->SetBranchAddress("electrons", &wfElectrons);

  // (event, aget, channel) -> the electrons and the bins they arrived in
  std::map<std::pair<int, std::pair<int, int>>, RawCharge> charge;
  for (Long64_t i = 0; i < waveforms->GetEntries(); ++i) {
    waveforms->GetEntry(i);
    RawCharge& cell = charge[{wfEventID, {wfAget, wfChGraw}}];
    cell.electrons += wfElectrons;
    if (wfBin < cell.firstBin) cell.firstBin = wfBin;
    if (wfBin > cell.lastBin) cell.lastBin = wfBin;
  }
  AtCheck(!charge.empty(), Form("%zu (event, channel) pairs saw electrons",
                                charge.size()));

  const std::vector<double> kernel = AgetKernel(peakingNs, 40.);
  double kernelArea = 0.;
  for (double h : kernel) kernelArea += h;
  std::printf("[info] kernel: %zu samples, peak %.4f, area %.4f\n",
              kernel.size(), *std::max_element(kernel.begin(), kernel.end()),
              kernelArea);

  // --- the ADC values ------------------------------------------------------
  Long64_t outOfRange = 0, badPedestalMean = 0, badPedestalRms = 0;
  Long64_t saturated = 0, nChannels = 0;
  Long64_t areaChecked = 0, areaFailed = 0;
  Long64_t loudChecked = 0, loudFailed = 0;
  double worstMean = 0., worstRms = noiseSigma;
  double pooledSum = 0., pooledSum2 = 0.;
  double worstArea = 0., worstAreaRatio = 1.;
  double totalArea = 0., totalExpected = 0.;
  int highest = 0, highestFpn = 0, nPulses = 0;
  Long64_t timeOutOfOrder = 0;
  ULong64_t previousTime = 0;

  for (Long64_t i = 0; i < raw->GetEntries(); ++i) {
    raw->GetEntry(i);
    if (i > 0 && eventTime <= previousTime) ++timeOutOfOrder;
    previousTime = eventTime;

    for (int a = 0; a < kNAget; ++a) {
      for (int c = 0; c < kNChannel; ++c) {
        double sum = 0., sum2 = 0.;
        for (int t = 0; t < kNCells; ++t) {
          const int value = adc[a][c][t];
          if (value < 0 || value > kAdcMax) ++outOfRange;
          if (value == kAdcMax) ++saturated;
          if (value > highest) highest = value;
          if (t >= kPedLo && t <= kPedHi) {
            sum += value;
            sum2 += static_cast<double>(value) * value;
          }
          if (IsFpnChannel(c) && value > highestFpn) highestFpn = value;
        }
        const int n = kPedHi - kPedLo + 1;
        const double mean = sum / n;
        const double rms = std::sqrt(std::max(sum2 / n - mean * mean, 0.));
        ++nChannels;
        pooledSum += sum;
        pooledSum2 += sum2;
        if (std::fabs(mean - pedestal) > 3. * noiseSigma) ++badPedestalMean;
        if (rms < 0.5 * noiseSigma || rms > 1.5 * noiseSigma) {
          ++badPedestalRms;
        }
        if (std::fabs(mean - pedestal) > std::fabs(worstMean)) {
          worstMean = mean - pedestal;
        }
        if (std::fabs(rms / noiseSigma - 1.) >
            std::fabs(worstRms / noiseSigma - 1.)) {
          worstRms = rms;
        }

        // The pulse: its area has to be the charge of Stage 3 shaped by h.
        const auto found =
            charge.find({static_cast<int>(eventId), {a, c}});
        if (found == charge.end()) continue;
        const RawCharge& cell = found->second;
        const double expected =
            AgetAdcOfCharge(AgetChargeFC(cell.electrons, gain), rangeFC) *
            kernelArea;
        double area = 0.;
        double peak = 0.;
        const int last = std::min(
            kNCells - 1, cell.lastBin + static_cast<int>(kernel.size()));
        for (int t = cell.firstBin; t <= last; ++t) {
          area += adc[a][c][t] - pedestal;
          peak = std::max(peak, adc[a][c][t] - pedestal);
        }
        if (peak > minPulseAdc) ++nPulses;
        totalArea += area;
        totalExpected += expected;

        // The pedestal comes off as a constant, so the noise of the summed
        // cells stays in the area: sigma * sqrt(cells). That is why the 5 %
        // of the specification carries a 3 sigma allowance on top.
        ++areaChecked;
        const double cells = last - cell.firstBin + 1;
        const double allowance = 3. * noiseSigma * std::sqrt(cells);
        if (std::fabs(area - expected) > 0.05 * expected + allowance) {
          ++areaFailed;
        }
        // The loud channels, where the noise is far below the 5 %, have to
        // keep the 5 % on their own.
        const double ratio = area / expected;
        if (expected > minAreaAdc) {
          ++loudChecked;
          if (std::fabs(ratio - 1.) > 0.05) ++loudFailed;
          if (std::fabs(ratio - 1.) > std::fabs(worstAreaRatio - 1.)) {
            worstAreaRatio = ratio;
            worstArea = expected;
          }
        }
      }
    }
  }

  AtCheck(outOfRange == 0,
          Form("every ADC value is 0..4095 (%lld are not)", outOfRange));
  AtCheck(timeOutOfOrder == 0,
          Form("eventTime increases (%lld entries do not)", timeOutOfOrder));
  AtCheck(badPedestalMean == 0,
          Form("every channel's cell %d..%d mean is within 3 sigma of the"
               " pedestal (%lld are not, worst %+.2f counts)",
               kPedLo, kPedHi, badPedestalMean, worstMean));
  // 20 cells give the rms of one channel a spread of 1/sqrt(2*19) = 16 %,
  // so 0.5..1.5 sigma is a 3.1 sigma cut and a handful of the thousands of
  // channels fall outside it by construction. The level of the noise is
  // checked on all of them pooled instead.
  AtCheck(badPedestalRms <= nChannels / 100,
          Form("%lld of %lld channels are outside 0.5..1.5 sigma in cells"
               " %d..%d (at most 1 %% allowed, worst %.2f counts for sigma"
               " %.2f)", badPedestalRms, nChannels, kPedLo, kPedHi, worstRms,
               noiseSigma));
  const double pooledCells = nChannels * (kPedHi - kPedLo + 1);
  const double pooledMean = pooledSum / pooledCells;
  const double pooledRms =
      std::sqrt(std::max(pooledSum2 / pooledCells - pooledMean * pooledMean,
                         0.));
  AtCheckNear(pooledMean, pedestal, 0.1, "pedestal of all channels pooled");
  AtCheckNear(pooledRms, noiseSigma, 0.05 * noiseSigma,
              "noise sigma of all channels pooled");
  AtCheck(highestFpn < pedestal + 5. * noiseSigma,
          Form("the FPN channels stay below pedestal + 5 sigma (highest %d,"
               " limit %.1f)", highestFpn, pedestal + 5. * noiseSigma));
  AtCheck(nPulses > 0,
          Form("%d channel(s) carry a pulse above pedestal + %.0f counts",
               nPulses, minPulseAdc));
  AtCheck(areaChecked > 0,
          Form("%lld pulse area(s) were compared with the shaped Stage 3"
               " charge", areaChecked));
  AtCheck(areaFailed == 0,
          Form("every pulse area is within 5 %% + 3 sigma of the shaped"
               " Stage 3 charge (%lld are not)", areaFailed));
  AtCheck(loudChecked > 0 && loudFailed == 0,
          Form("every one of the %lld pulses above %.0f counts is within"
               " 5 %% (%lld are not, worst ratio %.4f at %.0f counts)",
               loudChecked, minAreaAdc, loudFailed, worstAreaRatio,
               worstArea));
  AtCheckNear(totalArea / totalExpected, 1., 0.01,
              "summed pulse area / summed shaped Stage 3 charge");
  std::printf("[info] highest ADC value %d, %lld cell(s) at 4095\n", highest,
              saturated);
  AtReport();
}
