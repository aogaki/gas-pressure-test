#include "AgetUtils.hh"

#include <unistd.h>

#include <cstdlib>

namespace {

constexpr const char* kRootSuffix = ".root";
constexpr const char* kReadoutSuffix = "_readout";

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

// A positive number, for the options that have no meaning at zero.
bool ToPositive(const char* text, double& value) {
  return ToDouble(text, value) && value > 0.;
}

bool EndsWith(const std::string& text, const std::string& suffix) {
  return text.size() >= suffix.size() &&
         text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

}  // namespace

std::string AgetUsage() {
  return "usage: aget-shaper -i <stage3_readout.root> [-t <peaking ns>]"
         " [-r <range fC>] [-g <gain>] [-p <pedestal>] [-s <noise sigma>]"
         " [-n <maxEvents>] [-h]";
}

bool ParseAgetOptions(int argc, char* const argv[], AgetOptions& options,
                      std::string& error) {
  options = AgetOptions();
  error.clear();

  ResetGetopt();
  int c = 0;
  while ((c = getopt(argc, argv, "i:t:r:g:p:s:n:h")) != -1) {
    switch (c) {
      case 'i':
        options.input = optarg;
        break;
      case 't':
        if (!ToPositive(optarg, options.peakingNs)) {
          error = "-t needs a positive peaking time in nanoseconds";
          return false;
        }
        break;
      case 'r':
        if (!ToPositive(optarg, options.rangeFC)) {
          error = "-r needs a positive gain range in fC";
          return false;
        }
        break;
      case 'g':
        if (!ToPositive(optarg, options.gain)) {
          error = "-g needs a positive effective gain";
          return false;
        }
        break;
      case 'p':
        if (!ToDouble(optarg, options.pedestal) || options.pedestal < 0.) {
          error = "-p needs a pedestal of 0 or more ADC counts";
          return false;
        }
        break;
      case 's':
        if (!ToDouble(optarg, options.noiseSigma) || options.noiseSigma < 0.) {
          error = "-s needs a noise sigma of 0 or more ADC counts";
          return false;
        }
        break;
      case 'n':
        if (!ToLong(optarg, options.maxEvents) || options.maxEvents < 1) {
          error = "-n needs a positive number of events";
          return false;
        }
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
  return true;
}

std::string AgetOutputName(const std::string& input) {
  std::string stem = input;
  if (EndsWith(stem, kRootSuffix)) {
    stem.erase(stem.size() - std::string(kRootSuffix).size());
  }
  if (EndsWith(stem, kReadoutSuffix)) {
    stem.erase(stem.size() - std::string(kReadoutSuffix).size());
  }
  return stem + "_raw" + kRootSuffix;
}
