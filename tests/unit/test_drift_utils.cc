#include <gtest/gtest.h>

#include <cmath>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "DriftUtils.hh"

namespace {

// getopt() wants a mutable argv; the literals are never written to.
bool Parse(const std::vector<std::string>& args, DriftOptions& options,
           std::string& error) {
  std::vector<std::string> storage = args;
  std::vector<char*> argv;
  for (auto& arg : storage) argv.push_back(&arg[0]);
  argv.push_back(nullptr);
  return ParseDriftOptions(static_cast<int>(storage.size()), argv.data(),
                           options, error);
}

}  // namespace

TEST(ParseDriftOptionsTest, MandatoryOptionsOnly) {
  DriftOptions options;
  std::string error;
  ASSERT_TRUE(Parse({"drift-electrons", "-i", "in.root", "-v", "2000"}, options,
                    error))
      << error;
  EXPECT_EQ(options.input, "in.root");
  EXPECT_DOUBLE_EQ(options.voltage, 2000.);
  EXPECT_DOUBLE_EQ(options.fraction, 1.);
  EXPECT_LT(options.maxEvents, 0);
  EXPECT_EQ(options.cacheDir, "gasfiles");
  EXPECT_FALSE(options.help);
}

TEST(ParseDriftOptionsTest, AllOptions) {
  DriftOptions options;
  std::string error;
  ASSERT_TRUE(Parse({"drift-electrons", "-i", "a/in.root", "-v", "500.5", "-f",
                     "0.01", "-n", "7", "-c", "/tmp/gas"},
                    options, error))
      << error;
  EXPECT_EQ(options.input, "a/in.root");
  EXPECT_DOUBLE_EQ(options.voltage, 500.5);
  EXPECT_DOUBLE_EQ(options.fraction, 0.01);
  EXPECT_EQ(options.maxEvents, 7);
  EXPECT_EQ(options.cacheDir, "/tmp/gas");
}

TEST(ParseDriftOptionsTest, MissingMandatoryOptionsFail) {
  DriftOptions options;
  std::string error;
  EXPECT_FALSE(Parse({"drift-electrons"}, options, error));
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(Parse({"drift-electrons", "-v", "2000"}, options, error));
  EXPECT_FALSE(Parse({"drift-electrons", "-i", "in.root"}, options, error));
}

TEST(ParseDriftOptionsTest, OutOfRangeValuesFail) {
  DriftOptions options;
  std::string error;
  EXPECT_FALSE(
      Parse({"drift-electrons", "-i", "in.root", "-v", "0"}, options, error));
  EXPECT_FALSE(
      Parse({"drift-electrons", "-i", "in.root", "-v", "-100"}, options, error));
  EXPECT_FALSE(Parse({"drift-electrons", "-i", "in.root", "-v", "2000", "-f",
                      "0"},
                     options, error));
  EXPECT_FALSE(Parse({"drift-electrons", "-i", "in.root", "-v", "2000", "-f",
                      "1.5"},
                     options, error));
  EXPECT_FALSE(Parse({"drift-electrons", "-i", "in.root", "-v", "2000", "-n",
                      "0"},
                     options, error));
  EXPECT_FALSE(Parse({"drift-electrons", "-i", "in.root", "-v", "abc"}, options,
                     error));
}

TEST(ParseDriftOptionsTest, HelpNeedsNoOtherOption) {
  DriftOptions options;
  std::string error;
  ASSERT_TRUE(Parse({"drift-electrons", "-h"}, options, error)) << error;
  EXPECT_TRUE(options.help);
}

TEST(DriftUsageTest, MentionsTheMandatoryOptions) {
  const std::string usage = DriftUsage();
  EXPECT_NE(usage.find("-i"), std::string::npos);
  EXPECT_NE(usage.find("-v"), std::string::npos);
}

TEST(DriftOutputNameTest, InsertsTheVoltageBeforeTheSuffix) {
  EXPECT_EQ(DriftOutputName("Ar_200mbar_5.5MeV.root", 2000.),
            "Ar_200mbar_5.5MeV_2000V.root");
  EXPECT_EQ(DriftOutputName("dir/He_200mbar_0.3MeV.root", 1000.),
            "dir/He_200mbar_0.3MeV_1000V.root");
  EXPECT_EQ(DriftOutputName("in.root", 1500.5), "in_1500.5V.root");
}

TEST(DriftOutputNameTest, AppendsTheSuffixWhenTheInputHasNone) {
  EXPECT_EQ(DriftOutputName("in", 2000.), "in_2000V.root");
}

TEST(DriftFieldTest, VoltageOverTheDriftGap) {
  // 20 cm between the readout plane and the cathode.
  EXPECT_DOUBLE_EQ(DriftField(2000.), 100.);
  EXPECT_DOUBLE_EQ(DriftField(1000.), 50.);
  EXPECT_DOUBLE_EQ(DriftField(10000.), 500.);
}

TEST(MbarToTorrTest, KnownFactor) {
  EXPECT_DOUBLE_EQ(MbarToTorr(1.), 0.750062);
  EXPECT_NEAR(MbarToTorr(200.), 150.0124, 1e-9);
  EXPECT_NEAR(MbarToTorr(1013.25), 760.0003, 1e-3);
}

TEST(MagboltzGasNameTest, KnownGases) {
  EXPECT_EQ(MagboltzGasName("He"), "he");
  EXPECT_EQ(MagboltzGasName("Ar"), "ar");
  EXPECT_EQ(MagboltzGasName("CO2"), "co2");
}

TEST(MagboltzGasNameTest, UnknownGasThrows) {
  EXPECT_THROW(MagboltzGasName("Xe"), std::invalid_argument);
  EXPECT_THROW(MagboltzGasName(""), std::invalid_argument);
}

TEST(GasFileNameTest, ShortestNumbers) {
  EXPECT_EQ(GasFileName("gasfiles", "Ar", 200., 100.),
            "gasfiles/Ar_200mbar_100Vcm.gas");
  EXPECT_EQ(GasFileName("/tmp/c", "He", 1013.25, 12.5),
            "/tmp/c/He_1013.25mbar_12.5Vcm.gas");
}

TEST(SampleElectronCountTest, MeanAndVarianceFollowTheFanoFactor) {
  // 26.4 eV and F = 0.17 are the Magboltz values of argon.
  const double w = 26.4;
  const double fano = 0.17;
  const double edep = 0.0264;  // MeV, i.e. a mean of 1000 electrons
  const double expected = edep * 1e6 / w;
  std::mt19937 rng(1);
  const int n = 100000;
  double sum = 0., sum2 = 0.;
  for (int i = 0; i < n; ++i) {
    const double k = SampleElectronCount(edep, w, fano, rng);
    sum += k;
    sum2 += k * k;
  }
  const double mean = sum / n;
  const double variance = sum2 / n - mean * mean;
  EXPECT_NEAR(mean, expected, expected * 0.01);
  EXPECT_NEAR(variance, fano * expected, fano * expected * 0.10);
}

TEST(SampleElectronCountTest, NeverNegative) {
  std::mt19937 rng(2);
  for (int i = 0; i < 1000; ++i) {
    EXPECT_GE(SampleElectronCount(1e-8, 26.4, 0.17, rng), 0);
  }
  EXPECT_EQ(SampleElectronCount(0., 26.4, 0.17, rng), 0);
}

TEST(SampleDriftedCountTest, FullFractionKeepsEveryElectron) {
  std::mt19937 rng(3);
  EXPECT_EQ(SampleDriftedCount(1234, 1., rng), 1234);
  EXPECT_EQ(SampleDriftedCount(0, 1., rng), 0);
}

TEST(SampleDriftedCountTest, RandomRoundingIsUnbiased) {
  std::mt19937 rng(4);
  const int n = 100000;
  long sum = 0;
  for (int i = 0; i < n; ++i) sum += SampleDriftedCount(7, 0.1, rng);
  // 7 * 0.1 = 0.7, so plain rounding would give 1 every time.
  EXPECT_NEAR(static_cast<double>(sum) / n, 0.7, 0.02);
}
