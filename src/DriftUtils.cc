#include "DriftUtils.hh"

#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <stdexcept>

namespace {

constexpr double kTorrPerMbar = 0.750062;
constexpr const char* kRootSuffix = ".root";

// getopt() keeps state between calls, which the unit tests exercise.
void ResetGetopt() {
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || \
    defined(__NetBSD__)
  optreset = 1;
  optind = 1;
#else
  optind = 0;
#endif
}

// strtod() with a "the whole argument was a number" check.
bool ToDouble(const char* text, double& value) {
  char* end = nullptr;
  value = std::strtod(text, &end);
  return end != text && *end == '\0';
}

bool ToLong(const char* text, long& value) {
  char* end = nullptr;
  value = std::strtol(text, &end, 10);
  return end != text && *end == '\0';
}

}  // namespace

std::string DriftUsage() {
  return "usage: drift-electrons -i <input.root> -v <volt> [-f <fraction>]"
         " [-n <maxEvents>] [-c <cachedir>] [-h]";
}

bool ParseDriftOptions(int argc, char* const argv[], DriftOptions& options,
                       std::string& error) {
  options = DriftOptions();
  error.clear();
  bool hasVoltage = false;

  ResetGetopt();
  int c = 0;
  while ((c = getopt(argc, argv, "i:v:f:n:c:h")) != -1) {
    switch (c) {
      case 'i':
        options.input = optarg;
        break;
      case 'v':
        if (!ToDouble(optarg, options.voltage) || !(options.voltage > 0.)) {
          error = "-v needs a positive number of volts";
          return false;
        }
        hasVoltage = true;
        break;
      case 'f':
        if (!ToDouble(optarg, options.fraction) || !(options.fraction > 0.) ||
            options.fraction > 1.) {
          error = "-f needs a fraction in (0, 1]";
          return false;
        }
        break;
      case 'n':
        if (!ToLong(optarg, options.maxEvents) || options.maxEvents < 1) {
          error = "-n needs a positive number of events";
          return false;
        }
        break;
      case 'c':
        options.cacheDir = optarg;
        break;
      case 'h':
        options.help = true;
        return true;
      default:
        error = "unknown option";
        return false;
    }
  }

  if (options.input.empty()) {
    error = "missing -i";
    return false;
  }
  if (!hasVoltage) {
    error = "missing -v";
    return false;
  }
  return true;
}

std::string DriftFormatNumber(double value) {
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%g", value);
  return buffer;
}

std::string DriftOutputName(const std::string& input, double voltage) {
  std::string stem = input;
  const size_t suffix = std::string(kRootSuffix).size();
  if (stem.size() >= suffix && stem.compare(stem.size() - suffix, suffix,
                                            kRootSuffix) == 0) {
    stem.erase(stem.size() - suffix);
  }
  return stem + "_" + DriftFormatNumber(voltage) + "V" + kRootSuffix;
}

double DriftField(double voltage) { return voltage / kDriftGapCm; }

double MbarToTorr(double pressureMbar) { return pressureMbar * kTorrPerMbar; }

std::string MagboltzGasName(const std::string& gas) {
  static const std::map<std::string, std::string> names = {
      {"He", "he"}, {"Ar", "ar"}, {"CO2", "co2"}};
  const auto it = names.find(gas);
  if (it == names.end()) {
    throw std::invalid_argument("unknown gas: " + gas);
  }
  return it->second;
}

std::string GasFileName(const std::string& cacheDir, const std::string& gas,
                        double pressureMbar, double fieldVcm) {
  return cacheDir + "/" + gas + "_" + DriftFormatNumber(pressureMbar) +
         "mbar_" + DriftFormatNumber(fieldVcm) + "Vcm.gas";
}

int SampleElectronCount(double edepMeV, double wEv, double fano,
                        std::mt19937& rng) {
  if (!(edepMeV > 0.) || !(wEv > 0.)) return 0;
  const double mean = edepMeV * 1e6 / wEv;
  const double sigma = std::sqrt(std::max(fano, 0.) * mean);
  const double sampled =
      sigma > 0. ? std::normal_distribution<double>(mean, sigma)(rng) : mean;
  const long rounded = std::lround(sampled);
  return rounded > 0 ? static_cast<int>(rounded) : 0;
}

int SampleDriftedCount(int electrons, double fraction, std::mt19937& rng) {
  if (electrons <= 0) return 0;
  if (fraction >= 1.) return electrons;
  // Randomised rounding: floor(n * f + u) has the mean n * f exactly.
  const double expected = electrons * fraction;
  const double u = std::uniform_real_distribution<double>(0., 1.)(rng);
  const long count = static_cast<long>(std::floor(expected + u));
  return count > 0 ? static_cast<int>(count) : 0;
}
