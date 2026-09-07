#include "ChannelUtils.hh"

#include <unistd.h>

#include <cmath>

#include "CliUtils.hh"

namespace {

constexpr const char* kRootSuffix = ".root";

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
  if (EndsWith(stem, kRootSuffix)) {
    stem.erase(stem.size() - std::string(kRootSuffix).size());
  }
  return stem + "_readout" + kRootSuffix;
}

int TimeBin(double tNs, double windowStartUs) {
  const double offset = tNs - windowStartUs * 1000.;
  if (offset < 0.) return -1;
  const int bin = static_cast<int>(std::floor(offset / kBinNs));
  return bin < kNBins ? bin : -1;
}
