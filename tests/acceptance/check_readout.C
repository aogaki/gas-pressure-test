#include <map>
#include <set>
#include <string>
#include <utility>

#include "../../include/PadMap.hh"
#include "AtCheck.h"

// AT3-3: end to end. The electrons of a Stage 2 file land on the pads of
// geometry/pads.csv and are counted in 40 ns bins.
//
// The pad lookup of the application is reused (PadMap.hh is header-only), so
// this macro can redo the whole assignment from the Stage 2 file and compare
// it with what channel-response wrote.
//
// czLow / czHigh bracket the alpha track along the alpha-source axis: the
// pads that see electrons have to lie inside it. The band covers the range
// straggling of the alpha as well as the transverse diffusion of the
// electrons on their way down to the readout plane.
void check_readout(const char* readoutFile, const char* stage2File,
                   const char* padsCsv, const char* gas, double pressureMbar,
                   double voltage, int nEvents, double windowStartUs,
                   double minOnReadout, double minOnPads, double minInWindow,
                   double czLow, double czHigh) {
  TFile* file = AtOpen(readoutFile);

  // --- run: the Stage 2 columns plus the readout ones ----------------------
  TTree* run = AtTree(file, "run");
  AtCheck(run->GetEntries() == 1,
          Form("run has %lld entries (expected 1)", run->GetEntries()));
  char runGas[64] = {0};
  char padsFile[512] = {0};
  double runPressure = 0., runVoltage = 0., runWindow = 0., binNs = 0.;
  int nBins = 0;
  run->SetBranchAddress("gas", runGas);
  run->SetBranchAddress("pressure", &runPressure);
  run->SetBranchAddress("voltage", &runVoltage);
  run->SetBranchAddress("padsFile", padsFile);
  run->SetBranchAddress("windowStartUs", &runWindow);
  run->SetBranchAddress("binNs", &binNs);
  run->SetBranchAddress("nBins", &nBins);
  run->GetEntry(0);
  AtCheck(TString(runGas) == TString(gas),
          Form("run.gas = '%s' (expected '%s')", runGas, gas));
  AtCheckNear(runPressure, pressureMbar, 1e-9, "run.pressure [mbar]");
  AtCheckNear(runVoltage, voltage, 1e-9, "run.voltage [V]");
  AtCheck(TString(padsFile) == TString(padsCsv),
          Form("run.padsFile = '%s'", padsFile));
  AtCheckNear(runWindow, windowStartUs, 1e-9, "run.windowStartUs [us]");
  AtCheckNear(binNs, 40., 1e-9, "run.binNs [ns]");
  AtCheck(nBins == 512, Form("run.nBins = %d (expected 512)", nBins));

  // The events of Stage 1 have travelled all the way through.
  TTree* events = AtTree(file, "events");
  AtCheck(events->GetEntries() == nEvents,
          Form("events has %lld entries (expected %d)", events->GetEntries(),
               nEvents));

  // --- the pad map, as the application reads it ----------------------------
  PadMap pads(padsCsv);
  AtCheck(pads.Size() > 0, Form("pad map has %zu pads", pads.Size()));
  std::set<std::pair<int, int>> channels;
  std::map<std::pair<int, int>, std::pair<std::string, int>> strips;
  for (const Pad& pad : pads.Pads()) {
    channels.insert({pad.aget, pad.chGraw});
    strips[{pad.aget, pad.chGraw}] = {pad.stripDir, pad.stripNo};
  }

  // --- waveforms -----------------------------------------------------------
  TTree* waveforms = AtTree(file, "waveforms");
  int wfEventID = 0, aget = 0, chGraw = 0, stripNo = 0, bin = 0;
  char stripDir[8] = {0};
  double electrons = 0.;
  waveforms->SetBranchAddress("eventID", &wfEventID);
  waveforms->SetBranchAddress("aget", &aget);
  waveforms->SetBranchAddress("ch_graw", &chGraw);
  waveforms->SetBranchAddress("strip_dir", stripDir);
  waveforms->SetBranchAddress("strip_no", &stripNo);
  waveforms->SetBranchAddress("bin", &bin);
  waveforms->SetBranchAddress("electrons", &electrons);

  const Long64_t nRows = waveforms->GetEntries();
  AtCheck(nRows > 0, Form("waveforms has %lld rows", nRows));
  Long64_t badBin = 0, badChannel = 0, badStrip = 0, emptyRow = 0;
  int minBin = 1 << 30, maxBin = -1;
  double waveformSum = 0.;
  std::map<std::pair<int, int>, double> perChannel;
  for (Long64_t i = 0; i < nRows; ++i) {
    waveforms->GetEntry(i);
    if (bin < 0 || bin > 511) ++badBin;
    if (bin < minBin) minBin = bin;
    if (bin > maxBin) maxBin = bin;
    const std::pair<int, int> channel(aget, chGraw);
    if (channels.count(channel) == 0) {
      ++badChannel;
    } else if (strips[channel].first != std::string(stripDir) ||
               strips[channel].second != stripNo) {
      ++badStrip;
    }
    if (!(electrons > 0.)) ++emptyRow;
    waveformSum += electrons;
    perChannel[channel] += electrons;
  }
  AtCheck(badBin == 0, Form("every bin is 0..511 (%lld are not)", badBin));
  AtCheck(badChannel == 0,
          Form("every (aget, ch_graw) is in the pad map (%lld are not)",
               badChannel));
  AtCheck(badStrip == 0,
          Form("every strip_dir/strip_no matches the pad map (%lld do not)",
               badStrip));
  AtCheck(emptyRow == 0,
          Form("every row carries electrons (%lld do not)", emptyRow));
  AtCheck(static_cast<int>(perChannel.size()) <= 256,
          Form("%zu channels fired (at most 256 exist)", perChannel.size()));

  // --- summary -------------------------------------------------------------
  TTree* summary = AtTree(file, "summary");
  int sumEventID = 0;
  double total = 0., onReadout = 0., onPads = 0., inWindow = 0.;
  summary->SetBranchAddress("eventID", &sumEventID);
  summary->SetBranchAddress("total", &total);
  summary->SetBranchAddress("onReadout", &onReadout);
  summary->SetBranchAddress("onPads", &onPads);
  summary->SetBranchAddress("inWindow", &inWindow);
  AtCheck(summary->GetEntries() == nEvents,
          Form("summary has %lld rows (expected %d)", summary->GetEntries(),
               nEvents));

  double allTotal = 0., allOnReadout = 0., allOnPads = 0., allInWindow = 0.;
  Long64_t outOfOrder = 0;
  for (Long64_t i = 0; i < summary->GetEntries(); ++i) {
    summary->GetEntry(i);
    if (!(total >= onReadout && onReadout >= onPads && onPads >= inWindow &&
          inWindow >= 0.)) {
      ++outOfOrder;
    }
    allTotal += total;
    allOnReadout += onReadout;
    allOnPads += onPads;
    allInWindow += inWindow;
  }
  AtCheck(outOfOrder == 0,
          Form("every row has total >= onReadout >= onPads >= inWindow"
               " (%lld do not)", outOfOrder));
  AtCheckNear(waveformSum, allInWindow, 1e-6,
              "sum of waveforms.electrons vs sum of summary.inWindow");
  AtCheckRange(allOnReadout / allTotal, minOnReadout, 1.,
               "onReadout / total");
  AtCheckRange(allOnPads / allOnReadout, minOnPads, 1., "onPads / onReadout");
  AtCheckRange(allInWindow / allOnPads, minInWindow, 1., "inWindow / onPads");

  // --- the fired pads, redone from the Stage 2 electrons -------------------
  TFile* stage2 = AtOpen(stage2File);
  TTree* electronsIn = AtTree(stage2, "electrons");
  int eventID = 0;
  double x = 0., y = 0., z = 0., t = 0., weight = 0.;
  electronsIn->SetBranchAddress("eventID", &eventID);
  electronsIn->SetBranchAddress("x", &x);
  electronsIn->SetBranchAddress("y", &y);
  electronsIn->SetBranchAddress("z", &z);
  electronsIn->SetBranchAddress("t", &t);
  electronsIn->SetBranchAddress("weight", &weight);

  double redoneTotal = 0., redoneInWindow = 0.;
  double minCz = 1e30, maxCz = -1e30;
  const Long64_t nElectrons = electronsIn->GetEntries();
  for (Long64_t i = 0; i < nElectrons; ++i) {
    electronsIn->GetEntry(i);
    redoneTotal += weight;
    if (std::fabs(y + 100.) > 0.5) continue;
    const int index = pads.Find(x, z);
    if (index < 0) continue;
    const double offset = t - windowStartUs * 1000.;
    if (offset < 0. || offset >= 512. * 40.) continue;
    redoneInWindow += weight;
    const double cz = pads.Pads()[index].cz;
    if (cz < minCz) minCz = cz;
    if (cz > maxCz) maxCz = cz;
  }
  AtCheckNear(redoneTotal, allTotal, 1e-6,
              "sum of the Stage 2 weights vs sum of summary.total");
  AtCheckNear(redoneInWindow, allInWindow, 1e-6,
              "electrons in the window, redone from the Stage 2 file");
  AtCheckRange(minCz, czLow, czHigh, "lowest cz of a pad with electrons [mm]");
  AtCheckRange(maxCz, czLow, czHigh, "highest cz of a pad with electrons [mm]");
  std::printf("[info] fired pads: cz %.3f .. %.3f mm, bins %d .. %d,"
              " %zu channels, %lld rows\n",
              minCz, maxCz, minBin, maxBin, perChannel.size(), nRows);

  // The five busiest channels, for the report of TODO/06.
  std::vector<std::pair<double, std::pair<int, int>>> byElectrons;
  for (const auto& entry : perChannel) {
    byElectrons.push_back({entry.second, entry.first});
  }
  std::sort(byElectrons.begin(), byElectrons.end(),
            [](const std::pair<double, std::pair<int, int>>& a,
               const std::pair<double, std::pair<int, int>>& b) {
              return a.first > b.first;
            });
  for (std::size_t i = 0; i < byElectrons.size() && i < 5; ++i) {
    const std::pair<int, int>& channel = byElectrons[i].second;
    std::printf("[info] top channel %zu: aget %d ch_graw %d (%s%d) %.6g"
                " electrons\n",
                i + 1, channel.first, channel.second,
                strips[channel].first.c_str(), strips[channel].second,
                byElectrons[i].first);
  }

  AtReport();
}
