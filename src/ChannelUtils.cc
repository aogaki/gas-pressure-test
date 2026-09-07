#include "ChannelUtils.hh"

#include <unistd.h>

#include <cmath>
#include <cstdlib>

namespace {

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

std::string ChannelUsage() {
  return "usage: channel-response -i <stage2.root> -p <pads.csv> [-w <us>]"
         " [-n <maxEvents>] [-h]";
}

bool ParseChannelOptions(int argc, char* const argv[], ChannelOptions& options,
                         std::string& error) {
  options = ChannelOptions();
  error.clear();

  ResetGetopt();
  int c = 0;
  // The leading '-' of "-w -2.5" would look like an option, so getopt() needs
  // the ':' form; a negative window start is legal (the window may open
  // before the alpha).
  while ((c = getopt(argc, argv, "i:p:w:n:h")) != -1) {
    switch (c) {
      case 'i':
        options.input = optarg;
        break;
      case 'p':
        options.pads = optarg;
        break;
      case 'w':
        if (!ToDouble(optarg, options.windowStartUs)) {
          error = "-w needs a window start in microseconds";
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
  if (options.pads.empty()) {
    error = "missing -p";
    return false;
  }
  return true;
}

std::string ChannelOutputName(const std::string& input) {
  std::string stem = input;
  const std::size_t suffix = std::string(kRootSuffix).size();
  if (stem.size() >= suffix &&
      stem.compare(stem.size() - suffix, suffix, kRootSuffix) == 0) {
    stem.erase(stem.size() - suffix);
  }
  return stem + "_readout" + kRootSuffix;
}

int TimeBin(double tNs, double windowStartUs) {
  const double offset = tNs - windowStartUs * 1000.;
  if (offset < 0.) return -1;
  const int bin = static_cast<int>(std::floor(offset / kBinNs));
  return bin < kNBins ? bin : -1;
}
