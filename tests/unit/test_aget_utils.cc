#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "AgetResponse.hh"
#include "AgetUtils.hh"

namespace {

// The sampling of the AGET (TODO/06): 25 MHz, 512 cells.
constexpr double kBinNs = 40.;
constexpr int kNBins = 512;

// The index of the largest element.
std::size_t ArgMax(const std::vector<double>& v) {
  return static_cast<std::size_t>(
      std::max_element(v.begin(), v.end()) - v.begin());
}

// The response to N electrons in one time bin, in ADC counts (no pedestal).
std::vector<double> DeltaResponse(std::size_t bin, double electrons,
                                  double gain, double peakingNs,
                                  double rangeFC) {
  std::vector<double> charge(kNBins, 0.);
  charge[bin] = AgetChargeFC(electrons, gain);
  const std::vector<double> shaped =
      AgetConvolve(charge, AgetKernel(peakingNs, kBinNs));
  std::vector<double> adc(shaped.size());
  for (std::size_t k = 0; k < shaped.size(); ++k) {
    adc[k] = AgetAdcOfCharge(shaped[k], rangeFC);
  }
  return adc;
}

}  // namespace

// --- the shaping function ---------------------------------------------------

TEST(AgetResponseTest, IsOneAtThePeakAndZeroBeforeIt) {
  const double peaking = 223.;
  EXPECT_DOUBLE_EQ(AgetResponse(-1., peaking), 0.);
  EXPECT_DOUBLE_EQ(AgetResponse(0., peaking), 0.);
  EXPECT_NEAR(AgetResponse(peaking, peaking), 1., 1e-12);
  // 1.1664 tau is the peak, so nothing may rise above it.
  for (double t = 0.; t < 2000.; t += 1.) {
    EXPECT_LE(AgetResponse(t, peaking), 1. + 1e-12) << "t = " << t;
  }
}

TEST(AgetResponseTest, TauIsThePeakingTimeOverThePeakPosition) {
  EXPECT_NEAR(AgetTau(223.), 223. / 1.1664, 1e-12);
  // The zero crossing is at pi tau and the undershoot is about -1 %.
  const double tau = AgetTau(223.);
  EXPECT_NEAR(AgetResponse(M_PI * tau, 223.), 0., 1e-12);
  double lowest = 0.;
  for (double t = M_PI * tau; t < 5. * tau; t += 1.) {
    lowest = std::min(lowest, AgetResponse(t, 223.));
  }
  EXPECT_LT(lowest, 0.);
  EXPECT_NEAR(lowest, -0.01, 0.005);
}

TEST(AgetResponseTest, TheKernelStopsAtFiveTau) {
  const std::vector<double> kernel = AgetKernel(223., kBinNs);
  EXPECT_EQ(kernel.size(), 24u);  // 5 * 191.2 ns / 40 ns
  EXPECT_DOUBLE_EQ(kernel.front(), 0.);
  EXPECT_LT(std::fabs(kernel.back()), 0.01);
  // Twice the peaking time, twice as many samples.
  EXPECT_EQ(AgetKernel(446., kBinNs).size(), 48u);
}

// --- AT4-2 the response to one bin of electrons -----------------------------

TEST(AgetDeltaResponseTest, PeaksSixSamplesAfterTheCharge) {
  const std::size_t bin = 50;
  const double electrons = 100., gain = 1000., range = 120.;
  const std::vector<double> adc =
      DeltaResponse(bin, electrons, gain, 223., range);

  // 223 ns / 40 ns = 5.6 samples, and sample 6 is the closer one.
  const std::size_t peak = ArgMax(adc);
  EXPECT_EQ(peak, bin + 6);

  // The amplitude is the whole charge in ADC counts, up to the loss of the
  // sampling: the peak of the shaping falls between two samples.
  const double expected =
      AgetAdcOfCharge(AgetChargeFC(electrons, gain), range);
  EXPECT_NEAR(adc[peak], expected, 0.02 * expected);
  EXPECT_LT(adc[peak], expected);

  // Nothing before the charge (the response is causal).
  for (std::size_t k = 0; k < bin; ++k) EXPECT_DOUBLE_EQ(adc[k], 0.) << k;

  // 20 samples (800 ns) after the peak the pulse is over.
  for (std::size_t k = peak + 20; k < adc.size(); ++k) {
    EXPECT_LE(std::fabs(adc[k]), 0.02 * adc[peak]) << "sample " << k;
  }

  // The tail undershoots, and by no more than 2 % of the peak.
  const double lowest = *std::min_element(adc.begin(), adc.end());
  EXPECT_LT(lowest, 0.);
  EXPECT_LE(std::fabs(lowest), 0.02 * adc[peak]);
}

TEST(AgetDeltaResponseTest, TwiceThePeakingTimeIsTwiceAsLate) {
  const std::size_t bin = 50;
  const double electrons = 100., gain = 1000., range = 120.;
  const std::vector<double> fast =
      DeltaResponse(bin, electrons, gain, 223., range);
  const std::vector<double> slow =
      DeltaResponse(bin, electrons, gain, 446., range);

  const double fastDelay = ArgMax(fast) - bin;
  const double slowDelay = ArgMax(slow) - bin;
  EXPECT_NEAR(slowDelay / fastDelay, 2., 0.2);  // 11 samples against 6

  // The amplitude does not depend on the peaking time.
  EXPECT_NEAR(slow[ArgMax(slow)], fast[ArgMax(fast)],
              0.02 * fast[ArgMax(fast)]);
}

TEST(AgetConvolveTest, IsLinearAndCausal) {
  const std::vector<double> kernel = AgetKernel(223., kBinNs);
  std::vector<double> charge(kNBins, 0.);
  charge[10] = 1.;
  charge[13] = 2.;
  const std::vector<double> sum = AgetConvolve(charge, kernel);

  std::vector<double> first(kNBins, 0.), second(kNBins, 0.);
  first[10] = 1.;
  second[13] = 2.;
  const std::vector<double> a = AgetConvolve(first, kernel);
  const std::vector<double> b = AgetConvolve(second, kernel);
  for (std::size_t k = 0; k < sum.size(); ++k) {
    EXPECT_NEAR(sum[k], a[k] + b[k], 1e-12) << k;
  }
  EXPECT_EQ(sum.size(), static_cast<std::size_t>(kNBins));
  for (std::size_t k = 0; k < 10; ++k) EXPECT_DOUBLE_EQ(sum[k], 0.) << k;

  // The area of the shaped pulse is the charge times the area of the kernel.
  double area = 0., kernelArea = 0.;
  for (double v : a) area += v;
  for (double v : kernel) kernelArea += v;
  EXPECT_NEAR(area, kernelArea, 1e-9);
}

TEST(AgetChargeTest, OneElectronIsTheElementaryCharge) {
  EXPECT_NEAR(AgetChargeFC(1., 1.), 1.602e-4, 1e-12);
  EXPECT_NEAR(AgetChargeFC(10., 1000.), 1.602, 1e-9);
  // The full range is 4096 counts.
  EXPECT_NEAR(AgetAdcOfCharge(120., 120.), 4096., 1e-9);
  EXPECT_NEAR(AgetAdcOfCharge(60., 120.), 2048., 1e-9);
}

TEST(AgetFpnTest, FourChannelsPerAgetCarryNoSignal) {
  int fpn = 0;
  for (int ch = 0; ch < 68; ++ch) {
    if (IsFpnChannel(ch)) ++fpn;
  }
  EXPECT_EQ(fpn, 4);
  EXPECT_TRUE(IsFpnChannel(11));
  EXPECT_TRUE(IsFpnChannel(22));
  EXPECT_TRUE(IsFpnChannel(45));
  EXPECT_TRUE(IsFpnChannel(56));
  EXPECT_FALSE(IsFpnChannel(0));
  EXPECT_FALSE(IsFpnChannel(67));
}

// --- the command line and the output name -----------------------------------

namespace {

// getopt() wants a mutable argv; the literals are never written to.
bool Parse(const std::vector<std::string>& args, AgetOptions& options,
           std::string& error) {
  std::vector<std::string> storage = args;
  std::vector<char*> argv;
  for (auto& arg : storage) argv.push_back(&arg[0]);
  argv.push_back(nullptr);
  return ParseAgetOptions(static_cast<int>(storage.size()), argv.data(),
                          options, error);
}

}  // namespace

TEST(ParseAgetOptionsTest, TheDefaultsAreTheRunOf20260902) {
  AgetOptions options;
  std::string error;
  ASSERT_TRUE(Parse({"aget-shaper", "-i", "in_readout.root"}, options, error))
      << error;
  EXPECT_EQ(options.input, "in_readout.root");
  EXPECT_DOUBLE_EQ(options.peakingNs, 223.);
  EXPECT_DOUBLE_EQ(options.rangeFC, 120.);
  EXPECT_DOUBLE_EQ(options.gain, 3600.);
  EXPECT_DOUBLE_EQ(options.pedestal, 450.);
  EXPECT_DOUBLE_EQ(options.noiseSigma, 6.);
  EXPECT_LT(options.maxEvents, 0);
  EXPECT_FALSE(options.help);
}

TEST(ParseAgetOptionsTest, AllOptions) {
  AgetOptions options;
  std::string error;
  ASSERT_TRUE(Parse({"aget-shaper", "-i", "a/in_readout.root", "-t", "502",
                     "-r", "1000", "-g", "2500", "-p", "400", "-s", "9", "-n",
                     "3"},
                    options, error))
      << error;
  EXPECT_EQ(options.input, "a/in_readout.root");
  EXPECT_DOUBLE_EQ(options.peakingNs, 502.);
  EXPECT_DOUBLE_EQ(options.rangeFC, 1000.);
  EXPECT_DOUBLE_EQ(options.gain, 2500.);
  EXPECT_DOUBLE_EQ(options.pedestal, 400.);
  EXPECT_DOUBLE_EQ(options.noiseSigma, 9.);
  EXPECT_EQ(options.maxEvents, 3);
}

TEST(ParseAgetOptionsTest, TheInputIsMandatory) {
  AgetOptions options;
  std::string error;
  EXPECT_FALSE(Parse({"aget-shaper"}, options, error));
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(Parse({"aget-shaper", "-g", "1000"}, options, error));
}

TEST(ParseAgetOptionsTest, OutOfRangeValuesFail) {
  AgetOptions options;
  std::string error;
  const std::vector<std::vector<std::string>> bad = {
      {"aget-shaper", "-i", "in.root", "-t", "0"},
      {"aget-shaper", "-i", "in.root", "-t", "nonsense"},
      {"aget-shaper", "-i", "in.root", "-r", "-120"},
      {"aget-shaper", "-i", "in.root", "-g", "0"},
      {"aget-shaper", "-i", "in.root", "-p", "-1"},
      {"aget-shaper", "-i", "in.root", "-s", "-1"},
      {"aget-shaper", "-i", "in.root", "-n", "0"},
  };
  for (const auto& args : bad) {
    EXPECT_FALSE(Parse(args, options, error)) << args[3] << " " << args[4];
  }
}

TEST(ParseAgetOptionsTest, NoiseCanBeSwitchedOff) {
  AgetOptions options;
  std::string error;
  ASSERT_TRUE(Parse({"aget-shaper", "-i", "in.root", "-s", "0"}, options,
                    error))
      << error;
  EXPECT_DOUBLE_EQ(options.noiseSigma, 0.);
}

TEST(ParseAgetOptionsTest, HelpNeedsNoOtherOption) {
  AgetOptions options;
  std::string error;
  ASSERT_TRUE(Parse({"aget-shaper", "-h"}, options, error)) << error;
  EXPECT_TRUE(options.help);
}

TEST(AgetUsageTest, MentionsTheMandatoryOption) {
  EXPECT_NE(AgetUsage().find("-i"), std::string::npos);
}

TEST(AgetOutputNameTest, ReadoutBecomesRaw) {
  EXPECT_EQ(AgetOutputName("X_2000V_readout.root"), "X_2000V_raw.root");
  EXPECT_EQ(AgetOutputName("out/He_200mbar_0.3MeV_2000V_readout.root"),
            "out/He_200mbar_0.3MeV_2000V_raw.root");
}

TEST(AgetOutputNameTest, AnythingElseSimplyGetsRaw) {
  EXPECT_EQ(AgetOutputName("X_2000V.root"), "X_2000V_raw.root");
  EXPECT_EQ(AgetOutputName("X_2000V"), "X_2000V_raw.root");
}
