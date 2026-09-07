#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "ChannelUtils.hh"
#include "PadMap.hh"
#include "TFile.h"
#include "TTree.h"

namespace {

// The readout plane of TODO/01, and the tolerance check_drift.C uses to tell
// the electrons that arrived there from the ones that left sideways.
constexpr double kReadoutPlaneMm = -100.;
constexpr double kReadoutToleranceMm = 0.5;

// One (aget, ch_graw, bin) cell of the current event.
struct WaveformKey {
  int aget = 0;
  int chGraw = 0;
  int bin = 0;

  bool operator<(const WaveformKey& other) const {
    if (aget != other.aget) return aget < other.aget;
    if (chGraw != other.chGraw) return chGraw < other.chGraw;
    return bin < other.bin;
  }
};

// A channel is a strip, so (aget, ch_graw) fixes the strip of every pad on it.
struct StripId {
  std::string dir;
  int no = 0;
};

std::map<std::pair<int, int>, StripId> StripsOfChannels(const PadMap& pads) {
  std::map<std::pair<int, int>, StripId> strips;
  for (const Pad& pad : pads.Pads()) {
    strips[{pad.aget, pad.chGraw}] = StripId{pad.stripDir, pad.stripNo};
  }
  return strips;
}

}  // namespace

int main(int argc, char** argv) {
  ChannelOptions options;
  std::string error;
  if (!ParseChannelOptions(argc, argv, options, error)) {
    std::fprintf(stderr, "channel-response: %s\n%s\n", error.c_str(),
                 ChannelUsage().c_str());
    return 1;
  }
  if (options.help) {
    std::printf("%s\n", ChannelUsage().c_str());
    return 0;
  }

  std::unique_ptr<PadMap> pads;
  try {
    pads.reset(new PadMap(options.pads));
  } catch (const std::exception& e) {
    std::fprintf(stderr, "channel-response: %s\n", e.what());
    return 1;
  }
  const std::map<std::pair<int, int>, StripId> strips = StripsOfChannels(*pads);
  std::printf("channel-response: %zu pads, %zu channels from %s\n",
              pads->Size(), strips.size(), options.pads.c_str());

  TFile input(options.input.c_str());
  if (input.IsZombie()) {
    std::fprintf(stderr, "channel-response: cannot open %s\n",
                 options.input.c_str());
    return 1;
  }

  TTree* electronsIn = input.Get<TTree>("electrons");
  if (electronsIn == nullptr) {
    std::fprintf(stderr,
                 "channel-response: no 'electrons' ntuple in %s, this is not"
                 " a Stage 2 output\n",
                 options.input.c_str());
    return 1;
  }
  int inEventID = 0;
  double inX = 0., inY = 0., inZ = 0., inT = 0., inWeight = 1.;
  electronsIn->SetBranchAddress("eventID", &inEventID);
  electronsIn->SetBranchAddress("x", &inX);
  electronsIn->SetBranchAddress("y", &inY);
  electronsIn->SetBranchAddress("z", &inZ);
  electronsIn->SetBranchAddress("t", &inT);
  electronsIn->SetBranchAddress("weight", &inWeight);

  // The drift conditions of Stage 2 travel along, so that one file carries
  // the whole history of the run.
  TTree* runIn = input.Get<TTree>("run");
  if (runIn == nullptr || runIn->GetEntries() < 1) {
    std::fprintf(stderr, "channel-response: no 'run' ntuple in %s\n",
                 options.input.c_str());
    return 1;
  }
  char gas[64] = {0};
  double pressureMbar = 0., voltage = 0., efield = 0.;
  double vdrift = 0., dl = 0., dt = 0., w = 0., fano = 0.;
  runIn->SetBranchAddress("gas", gas);
  runIn->SetBranchAddress("pressure", &pressureMbar);
  runIn->SetBranchAddress("voltage", &voltage);
  runIn->SetBranchAddress("efield", &efield);
  runIn->SetBranchAddress("vdrift", &vdrift);
  runIn->SetBranchAddress("dl", &dl);
  runIn->SetBranchAddress("dt", &dt);
  runIn->SetBranchAddress("w", &w);
  runIn->SetBranchAddress("fano", &fano);
  runIn->GetEntry(0);

  const std::string outputName = ChannelOutputName(options.input);
  TFile output(outputName.c_str(), "RECREATE");
  if (output.IsZombie()) {
    std::fprintf(stderr, "channel-response: cannot write %s\n",
                 outputName.c_str());
    return 1;
  }

  // The events of Stage 1 travel along unchanged, for the analysis.
  TTree* eventsIn = input.Get<TTree>("events");
  if (eventsIn != nullptr) {
    output.cd();
    eventsIn->CloneTree(-1, "fast");
  }

  output.cd();
  int wfEventID = 0, wfAget = 0, wfChGraw = 0, wfStripNo = 0, wfBin = 0;
  char wfStripDir[8] = {0};
  double wfElectrons = 0.;
  TTree* waveforms =
      new TTree("waveforms", "One row per (event, channel, time bin)");
  waveforms->Branch("eventID", &wfEventID, "eventID/I");
  waveforms->Branch("aget", &wfAget, "aget/I");
  waveforms->Branch("ch_graw", &wfChGraw, "ch_graw/I");
  waveforms->Branch("strip_dir", wfStripDir, "strip_dir/C");
  waveforms->Branch("strip_no", &wfStripNo, "strip_no/I");
  waveforms->Branch("bin", &wfBin, "bin/I");
  waveforms->Branch("electrons", &wfElectrons, "electrons/D");

  int sumEventID = 0;
  double sumTotal = 0., sumOnReadout = 0., sumOnPads = 0., sumInWindow = 0.;
  TTree* summary = new TTree("summary", "One row per event");
  summary->Branch("eventID", &sumEventID, "eventID/I");
  summary->Branch("total", &sumTotal, "total/D");
  summary->Branch("onReadout", &sumOnReadout, "onReadout/D");
  summary->Branch("onPads", &sumOnPads, "onPads/D");
  summary->Branch("inWindow", &sumInWindow, "inWindow/D");

  // The electrons of Stage 2 are written in event order, so one event's cells
  // are gathered in this map and flushed when the next event begins.
  std::map<WaveformKey, double> cells;
  bool haveEvent = false;
  long long rows = 0;
  double allTotal = 0., allOnReadout = 0., allOnPads = 0., allInWindow = 0.;

  auto flush = [&]() {
    if (!haveEvent) return;
    wfEventID = sumEventID;
    for (const auto& cell : cells) {
      wfAget = cell.first.aget;
      wfChGraw = cell.first.chGraw;
      wfBin = cell.first.bin;
      wfElectrons = cell.second;
      const auto strip = strips.find({wfAget, wfChGraw});
      std::snprintf(wfStripDir, sizeof(wfStripDir), "%s",
                    strip != strips.end() ? strip->second.dir.c_str() : "?");
      wfStripNo = strip != strips.end() ? strip->second.no : -1;
      waveforms->Fill();
      ++rows;
    }
    summary->Fill();
    allTotal += sumTotal;
    allOnReadout += sumOnReadout;
    allOnPads += sumOnPads;
    allInWindow += sumInWindow;
    cells.clear();
  };

  const Long64_t nElectrons = electronsIn->GetEntries();
  for (Long64_t i = 0; i < nElectrons; ++i) {
    electronsIn->GetEntry(i);
    if (options.maxEvents > 0 && inEventID >= options.maxEvents) continue;
    if (!haveEvent || inEventID != sumEventID) {
      flush();
      haveEvent = true;
      sumEventID = inEventID;
      sumTotal = sumOnReadout = sumOnPads = sumInWindow = 0.;
    }

    sumTotal += inWeight;
    if (std::fabs(inY - kReadoutPlaneMm) > kReadoutToleranceMm) continue;
    sumOnReadout += inWeight;

    const int pad = pads->Find(inX, inZ);
    if (pad < 0) continue;
    sumOnPads += inWeight;

    const int bin = TimeBin(inT, options.windowStartUs);
    if (bin < 0) continue;
    sumInWindow += inWeight;

    const Pad& hit = pads->Pads()[pad];
    cells[WaveformKey{hit.aget, hit.chGraw, bin}] += inWeight;
  }
  flush();

  TTree* runOut = new TTree("run", "One row with the run conditions");
  char padsFile[512] = {0};
  std::snprintf(padsFile, sizeof(padsFile), "%s", options.pads.c_str());
  double windowStartUs = options.windowStartUs;
  double binNs = kBinNs;
  int nBins = kNBins;
  runOut->Branch("gas", gas, "gas/C");
  runOut->Branch("pressure", &pressureMbar, "pressure/D");
  runOut->Branch("voltage", &voltage, "voltage/D");
  runOut->Branch("efield", &efield, "efield/D");
  runOut->Branch("vdrift", &vdrift, "vdrift/D");
  runOut->Branch("dl", &dl, "dl/D");
  runOut->Branch("dt", &dt, "dt/D");
  runOut->Branch("w", &w, "w/D");
  runOut->Branch("fano", &fano, "fano/D");
  runOut->Branch("padsFile", padsFile, "padsFile/C");
  runOut->Branch("windowStartUs", &windowStartUs, "windowStartUs/D");
  runOut->Branch("binNs", &binNs, "binNs/D");
  runOut->Branch("nBins", &nBins, "nBins/I");
  runOut->Fill();

  output.Write();
  output.Close();

  std::printf(
      "channel-response: %.6g electrons, %.6g on the readout plane, %.6g on"
      " pads, %.6g in the window -> %lld rows in %s\n",
      allTotal, allOnReadout, allOnPads, allInWindow, rows,
      outputName.c_str());
  return 0;
}
