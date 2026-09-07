#pragma once

#include <string>

// Command line and file naming of Stage 4 (aget-shaper), free of any ROOT,
// Geant4 and Garfield++ dependency. The response itself is in
// AgetResponse.hh.

// Command line of aget-shaper. The defaults are the settings of the mini-eTPC
// run of 2026-09-02 (TODO/07); the gain is the effective GEM gain calibrated
// against the 241Am data of that run (TODO/08, 2026-09-07).
struct AgetOptions {
  std::string input;        // -i, mandatory: the Stage 3 output
  double peakingNs = 223.;  // -t, AGET peaking time [ns]
  double rangeFC = 120.;    // -r, AGET gain range (full scale) [fC]
  double gain = 3600.;      // -g, effective GEM gain (241Am, TODO/08)
  double pedestal = 450.;   // -p, pedestal [ADC counts]
  double noiseSigma = 6.;   // -s, noise sigma [ADC counts]
  long maxEvents = -1;      // -n, negative means "all events"
  bool help = false;        // -h
};

// Parses the command line. Returns false and fills 'error' when an option is
// missing or out of range. With -h it returns true and sets options.help.
bool ParseAgetOptions(int argc, char* const argv[], AgetOptions& options,
                      std::string& error);

// Usage text, without a trailing newline.
std::string AgetUsage();

// Output name: the "_readout" of the Stage 3 file becomes "_raw", so that
// analyzeUVW.C finds the "_raw.root" it replaces with "_uvw.root". An input
// that does not carry "_readout" simply gets "_raw" appended.
std::string AgetOutputName(const std::string& input);
