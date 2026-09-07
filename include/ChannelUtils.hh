#pragma once

#include <string>

// Pure helpers of Stage 3 (channel-response), free of any ROOT, Geant4 and
// Garfield++ dependency.

// The AGET sampling of TODO/06: 25 MHz, 512 cells per event = 20.48 us.
constexpr double kBinNs = 40.;
constexpr int kNBins = 512;

// Command line of channel-response.
struct ChannelOptions {
  std::string input;          // -i, mandatory: the Stage 2 output
  std::string pads;           // -p, mandatory: geometry/pads.csv
  double windowStartUs = 0.;  // -w, start of the 512 cell window [us]
  long maxEvents = -1;        // -n, negative means "all events"
  bool help = false;          // -h
};

// Parses the command line. Returns false and fills 'error' when an option is
// missing or out of range. With -h it returns true and sets options.help.
bool ParseChannelOptions(int argc, char* const argv[], ChannelOptions& options,
                         std::string& error);

// Usage text, without a trailing newline.
std::string ChannelUsage();

// Output name: "_readout" inserted in front of the ".root" suffix.
std::string ChannelOutputName(const std::string& input);

// Time bin of an arrival time t [ns] in the window that opens at
// windowStartUs [us]: floor((t - w0) / 40 ns). Returns -1 for a time outside
// the 512 cells of the window.
int TimeBin(double tNs, double windowStartUs);
