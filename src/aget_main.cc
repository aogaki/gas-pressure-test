#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "AgetResponse.hh"
#include "AgetUtils.hh"
#include "TFile.h"
#include "TTree.h"

namespace {

// The same fixed seed as Stage 2: a run is reproducible (TODO/07).
constexpr unsigned int kRandomSeed = 12345;

// The shape of the GRAW data of graw2root.C.
constexpr int kNAget = 4;
constexpr int kNChannel = 68;
constexpr int kNCells = 512;

// The sampling of the AGET (TODO/06): 25 MHz.
constexpr double kBinNs = 40.;

// A monotonic stand-in for the 48 bit CoBo timestamp.
constexpr unsigned long long kTicksPerEvent = 100000000ULL;

// One (aget, ch_graw) channel.
using Channel = std::pair<int, int>;

// One row of the Stage 3 "waveforms" ntuple, as charge [fC].
struct ChargeCell {
  int aget = 0;
  int chGraw = 0;
  int bin = 0;
  double chargeFC = 0.;
};

// Reads the eventIDs of an ntuple that has one row per event, in file order
// and without repetition.
std::vector<int> EventIds(TTree* tree, long maxEvents) {
  std::vector<int> events;
  std::set<int> seen;
  if (tree == nullptr) return events;
  int eventID = 0;
  tree->SetBranchAddress("eventID", &eventID);
  for (Long64_t i = 0; i < tree->GetEntries(); ++i) {
    tree->GetEntry(i);
    if (maxEvents > 0 && eventID >= maxEvents) continue;
    if (seen.insert(eventID).second) events.push_back(eventID);
  }
  tree->ResetBranchAddresses();
  return events;
}

}  // namespace

int main(int argc, char** argv) {
  AgetOptions options;
  std::string error;
  if (!ParseAgetOptions(argc, argv, options, error)) {
    std::fprintf(stderr, "aget-shaper: %s\n%s\n", error.c_str(),
                 AgetUsage().c_str());
    return 1;
  }
  if (options.help) {
    std::printf("%s\n", AgetUsage().c_str());
    return 0;
  }

  TFile input(options.input.c_str());
  if (input.IsZombie()) {
    std::fprintf(stderr, "aget-shaper: cannot open %s\n",
                 options.input.c_str());
    return 1;
  }

  TTree* waveforms = input.Get<TTree>("waveforms");
  if (waveforms == nullptr) {
    std::fprintf(stderr,
                 "aget-shaper: no 'waveforms' ntuple in %s, this is not a"
                 " Stage 3 output\n",
                 options.input.c_str());
    return 1;
  }

  // Every event of the run gets an entry, even one without electrons. The
  // event list is the one of Stage 3 ("summary", one row per event); a file
  // without it falls back to the Stage 1 events.
  std::vector<int> events =
      EventIds(input.Get<TTree>("summary"), options.maxEvents);
  if (events.empty()) {
    events = EventIds(input.Get<TTree>("events"), options.maxEvents);
  }

  // The electrons of one event, per channel and time cell. The whole ntuple
  // is read at once because the events are written out in order.
  int wfEventID = 0, wfAget = 0, wfChGraw = 0, wfBin = 0;
  double wfElectrons = 0.;
  waveforms->SetBranchAddress("eventID", &wfEventID);
  waveforms->SetBranchAddress("aget", &wfAget);
  waveforms->SetBranchAddress("ch_graw", &wfChGraw);
  waveforms->SetBranchAddress("bin", &wfBin);
  waveforms->SetBranchAddress("electrons", &wfElectrons);

  // The rows are kept as they come, one small record each: a run of many
  // events would need gigabytes if every channel held all 512 cells here.
  std::map<int, std::vector<ChargeCell>> charge;
  std::set<int> waveformEvents;
  long long droppedFpn = 0, droppedRange = 0;
  const Long64_t nRows = waveforms->GetEntries();
  for (Long64_t i = 0; i < nRows; ++i) {
    waveforms->GetEntry(i);
    if (options.maxEvents > 0 && wfEventID >= options.maxEvents) continue;
    waveformEvents.insert(wfEventID);
    if (wfAget < 0 || wfAget >= kNAget || wfChGraw < 0 ||
        wfChGraw >= kNChannel || wfBin < 0 || wfBin >= kNCells) {
      ++droppedRange;
      continue;
    }
    if (IsFpnChannel(wfChGraw)) {  // no signal on the FPN channels
      ++droppedFpn;
      continue;
    }
    charge[wfEventID].push_back(ChargeCell{
        wfAget, wfChGraw, wfBin, AgetChargeFC(wfElectrons, options.gain)});
  }
  if (events.empty()) {  // neither "summary" nor "events": use what fired
    events.assign(waveformEvents.begin(), waveformEvents.end());
  }

  const std::string outputName = AgetOutputName(options.input);
  TFile output(outputName.c_str(), "RECREATE");
  if (output.IsZombie()) {
    std::fprintf(stderr, "aget-shaper: cannot write %s\n", outputName.c_str());
    return 1;
  }

  // The tree of graw2root.C, column for column, so that analyzeUVW.C and
  // makeTracks.C run on this file exactly as they do on real data.
  output.cd();
  TTree* raw = new TTree("raw", "mini-eTPC raw waveforms");
  UInt_t eventId = 0;
  ULong64_t eventTime = 0;
  static Short_t adc[kNAget][kNChannel][kNCells];
  raw->Branch("eventId", &eventId);
  raw->Branch("eventTime", &eventTime);
  raw->Branch("adc", adc, "adc[4][68][512]/S");

  const std::vector<double> kernel = AgetKernel(options.peakingNs, kBinNs);
  std::mt19937 rng(kRandomSeed);
  // std::normal_distribution wants a positive sigma, so "-s 0" switches the
  // noise off instead of asking for a zero-wide Gaussian.
  const bool withNoise = options.noiseSigma > 0.;
  std::normal_distribution<double> noise(0.,
                                         withNoise ? options.noiseSigma : 1.);

  long long saturated = 0, clipped = 0;
  int highest = 0;
  for (int event : events) {
    eventId = static_cast<UInt_t>(event);
    eventTime = static_cast<ULong64_t>(event) * kTicksPerEvent;

    // The shaped signal of every channel that saw electrons, in ADC counts.
    std::map<Channel, std::vector<double>> shaped;
    const auto found = charge.find(event);
    if (found != charge.end()) {
      for (const ChargeCell& cell : found->second) {
        std::vector<double>& cells = shaped[{cell.aget, cell.chGraw}];
        if (cells.empty()) cells.resize(kNCells, 0.);
        cells[cell.bin] += cell.chargeFC;
      }
      for (auto& entry : shaped) {
        entry.second = AgetConvolve(entry.second, kernel);
        for (double& value : entry.second) {
          value = AgetAdcOfCharge(value, options.rangeFC);
        }
      }
    }

    for (int a = 0; a < kNAget; ++a) {
      for (int c = 0; c < kNChannel; ++c) {
        const auto signal = shaped.find({a, c});
        const std::vector<double>* counts =
            signal == shaped.end() ? nullptr : &signal->second;
        for (int t = 0; t < kNCells; ++t) {
          double value = options.pedestal;
          if (withNoise) value += noise(rng);
          if (counts != nullptr) value += (*counts)[t];
          long rounded = std::lround(value);
          if (rounded > kAdcMax) {
            ++saturated;
            rounded = kAdcMax;
          } else if (rounded < 0) {
            ++clipped;
            rounded = 0;
          }
          if (rounded > highest) highest = static_cast<int>(rounded);
          adc[a][c][t] = static_cast<Short_t>(rounded);
        }
      }
    }
    raw->Fill();
  }

  output.Write();
  output.Close();

  std::printf(
      "aget-shaper: %zu events, %lld waveform rows (%lld on FPN channels,"
      " %lld out of range)\n",
      events.size(), nRows, droppedFpn, droppedRange);
  std::printf(
      "aget-shaper: peaking %g ns (tau %.1f ns, %zu kernel samples), range"
      " %g fC, gain %g, pedestal %g, noise %g -> %s\n",
      options.peakingNs, AgetTau(options.peakingNs), kernel.size(),
      options.rangeFC, options.gain, options.pedestal, options.noiseSigma,
      outputName.c_str());
  std::printf(
      "aget-shaper: highest ADC value %d, %lld cell(s) saturated at %d,"
      " %lld cell(s) clipped at 0\n",
      highest, saturated, kAdcMax, clipped);
  return 0;
}
