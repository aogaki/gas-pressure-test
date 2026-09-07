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
#include "ChannelUtils.hh"
#include "TFile.h"
#include "TTree.h"

namespace {

// The same fixed seed as Stage 2: a run is reproducible (TODO/07).
constexpr unsigned int kRandomSeed = 12345;

// A monotonic stand-in for the 48 bit CoBo timestamp.
constexpr unsigned long long kTicksPerEvent = 100000000ULL;

// One (aget, ch_graw) channel.
using Channel = std::pair<int, int>;

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

  // Every event of the run gets an entry, even one without electrons, so the
  // entries of "raw" match the triggers of the real detector one for one.
  // The list is the Stage 1 "events" ntuple, which Stage 2 and Stage 3 carry
  // along; a file without it falls back to the Stage 3 "summary" (one row per
  // event that had electrons) and then to the events "waveforms" itself
  // shows (TODO/09 R6).
  std::vector<int> events =
      EventIds(input.Get<TTree>("events"), options.maxEvents);
  if (events.empty()) {
    events = EventIds(input.Get<TTree>("summary"), options.maxEvents);
  }
  if (events.empty()) {
    events = EventIds(waveforms, options.maxEvents);
  }

  // One row of the Stage 3 "waveforms" ntuple: the electrons of one channel
  // in one time cell.
  int wfEventID = 0, wfAget = 0, wfChGraw = 0, wfBin = 0;
  double wfElectrons = 0.;
  waveforms->SetBranchAddress("eventID", &wfEventID);
  waveforms->SetBranchAddress("aget", &wfAget);
  waveforms->SetBranchAddress("ch_graw", &wfChGraw);
  waveforms->SetBranchAddress("bin", &wfBin);
  waveforms->SetBranchAddress("electrons", &wfElectrons);

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

  // Stage 3 writes the rows of "waveforms" in eventID order and the event
  // list is in that order too, so one pass through the tree serves the whole
  // run: for every event the rows are consumed while their eventID matches,
  // and only the channels of the event being written are ever held in memory
  // (TODO/09 R18).
  const Long64_t nRows = waveforms->GetEntries();
  Long64_t row = 0;
  long long droppedFpn = 0, droppedRange = 0, droppedUnlisted = 0;
  long long saturated = 0, clipped = 0;
  int highest = 0;
  for (int event : events) {
    eventId = static_cast<UInt_t>(event);
    eventTime = static_cast<ULong64_t>(event) * kTicksPerEvent;

    // The signal of every channel that saw electrons, first as charge [fC]
    // and then, once the event is complete, shaped into ADC counts.
    std::map<Channel, std::vector<double>> shaped;
    while (row < nRows) {
      waveforms->GetEntry(row);
      if (wfEventID > event) break;  // a later event, left for its own turn
      ++row;
      if (wfEventID != event) {  // an event the list does not know about
        ++droppedUnlisted;
        continue;
      }
      if (wfAget < 0 || wfAget >= kNAget || wfChGraw < 0 ||
          wfChGraw >= kNChannel || wfBin < 0 || wfBin >= kNCells) {
        ++droppedRange;
        continue;
      }
      if (IsFpnChannel(wfChGraw)) {  // no signal on the FPN channels
        ++droppedFpn;
        continue;
      }
      std::vector<double>& cells = shaped[{wfAget, wfChGraw}];
      if (cells.empty()) cells.resize(kNCells, 0.);
      cells[wfBin] += AgetChargeFC(wfElectrons, options.gain);
    }
    for (auto& entry : shaped) {
      entry.second = AgetConvolve(entry.second, kernel);
      for (double& value : entry.second) {
        value = AgetAdcOfCharge(value, options.rangeFC);
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

  // Whatever is left behind the last event of the list: the rows -n left out,
  // and any event the list does not know about.
  while (row < nRows) {
    waveforms->GetEntry(row);
    ++row;
    if (options.maxEvents > 0 && wfEventID >= options.maxEvents) continue;
    ++droppedUnlisted;
  }

  output.Write();
  output.Close();

  std::printf(
      "aget-shaper: %zu events, %lld waveform rows (%lld on FPN channels,"
      " %lld out of range)\n",
      events.size(), nRows, droppedFpn, droppedRange);
  if (droppedUnlisted > 0) {
    std::fprintf(stderr,
                 "aget-shaper: warning: %lld waveform row(s) belong to an"
                 " event that is not in the event list and were dropped\n",
                 droppedUnlisted);
  }
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
