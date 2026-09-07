#pragma once

#include <random>
#include <string>

// Pure helpers of Stage 2 (drift-electrons), free of any Garfield++, ROOT or
// Geant4 dependency.

// Drift gap of the TPC [cm]: y = -10 cm (readout) to y = +10 cm (cathode).
constexpr double kDriftGapCm = 20.;

// Command line of drift-electrons.
struct DriftOptions {
  std::string input;               // -i, mandatory
  double voltage = 0.;             // -v [V], mandatory
  double fraction = 1.;            // -f, 0 < f <= 1
  long maxEvents = -1;             // -n, negative means "all events"
  std::string cacheDir = "gasfiles";  // -c
  bool help = false;               // -h
};

// Parses the command line. Returns false and fills 'error' when an option is
// missing or out of range. With -h it returns true and sets options.help.
bool ParseDriftOptions(int argc, char* const argv[], DriftOptions& options,
                       std::string& error);

// Usage text, without a trailing newline.
std::string DriftUsage();

// Shortest "%g" style representation of a number.
std::string DriftFormatNumber(double value);

// Output name: "_{V}V" inserted in front of the ".root" suffix of the input.
std::string DriftOutputName(const std::string& input, double voltage);

// Drift field [V/cm] of a voltage [V] applied across the drift gap.
double DriftField(double voltage);

// Pressure conversion, 1 mbar = 0.750062 Torr.
double MbarToTorr(double pressureMbar);

// Magboltz name of a single gas of Stage 1 ("He" -> "he", "Ar" -> "ar",
// "CO2" -> "co2"). Throws std::invalid_argument for an unknown gas.
std::string MagboltzGasName(const std::string& gas);

// A gas specification in the form MediumMagboltz::SetComposition() wants:
// six (Magboltz name, percentage) pairs, the unused slots left empty.
struct MagboltzMix {
  static constexpr int kMaxComponents = 6;
  std::string names[kMaxComponents];
  double fractions[kMaxComponents] = {0., 0., 0., 0., 0., 0.};
};

// Composition of a Stage 1 gas specification ("He-90-CO2-10").
// Throws std::invalid_argument for a bad specification.
MagboltzMix MagboltzComposition(const std::string& gasSpec);

// Cache file of the gas table: "{cacheDir}/{gas}_{p}mbar_{E}Vcm.gas".
std::string GasFileName(const std::string& cacheDir, const std::string& gas,
                        double pressureMbar, double fieldVcm);

// Number of ionisation electrons of one hit: a Gaussian around edep / W with
// the Fano-limited width sqrt(F * n), rounded and never negative.
int SampleElectronCount(double edepMeV, double wEv, double fano,
                        std::mt19937& rng);

// Number of electrons to drift out of 'electrons' when only the fraction
// 'fraction' is followed. Rounds randomly so that the mean stays exact.
int SampleDriftedCount(int electrons, double fraction, std::mt19937& rng);
