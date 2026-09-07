#include <gtest/gtest.h>

#include <cmath>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "ChannelUtils.hh"
#include "PadMap.hh"

#ifndef PADS_CSV_PATH
#error "PADS_CSV_PATH must be defined by the build"
#endif

namespace {

// The real pad map is read once: 13,010 pads take a moment to parse.
const PadMap& Pads() {
  static const PadMap map(PADS_CSV_PATH);
  return map;
}

// A single rhombic pad, counter-clockwise, centred on (0, 0).
Pad UnitPad() {
  Pad pad;
  pad.padId = 7;
  pad.stripDir = "U";
  pad.stripNo = 3;
  pad.cx = 0.;
  pad.cz = 0.;
  const double h = std::sqrt(3.) / 2.;
  pad.x[0] = 0.;
  pad.z[0] = -h;
  pad.x[1] = 0.5;
  pad.z[1] = 0.;
  pad.x[2] = 0.;
  pad.z[2] = h;
  pad.x[3] = -0.5;
  pad.z[3] = 0.;
  return pad;
}

}  // namespace

TEST(PadContainsTest, CentreIsInside) {
  const Pad pad = UnitPad();
  EXPECT_TRUE(PadContains(pad, 0., 0.));
}

TEST(PadContainsTest, VertexAndEdgeCount) {
  const Pad pad = UnitPad();
  EXPECT_TRUE(PadContains(pad, pad.x[1], pad.z[1]));       // vertex
  EXPECT_TRUE(PadContains(pad, 0.25, -std::sqrt(3.) / 4.));  // edge midpoint
}

TEST(PadContainsTest, OutsidePointsAreRejected) {
  const Pad pad = UnitPad();
  EXPECT_FALSE(PadContains(pad, 0.6, 0.));
  EXPECT_FALSE(PadContains(pad, 0., 1.));
  EXPECT_FALSE(PadContains(pad, 0.4, 0.6));  // outside the slanted edge
}

TEST(PadMapTest, ReadsEveryPadOfTheRealMap) {
  const PadMap& map = Pads();
  EXPECT_EQ(map.Size(), 13010u);
  // The plane of TODO/06: 107.5 mm across x, 106.5 mm along the
  // alpha-source axis.
  EXPECT_NEAR(map.MinX(), -53.75, 1e-6);
  EXPECT_NEAR(map.MaxX(), 53.75, 1e-6);
  EXPECT_NEAR(map.MinZ(), -250., 1e-6);
  EXPECT_NEAR(map.MaxZ(), -143.479, 1e-6);

  const Pad& first = map.Pads().front();
  EXPECT_EQ(first.padId, 0);
  EXPECT_EQ(first.stripDir, "U");
  EXPECT_EQ(first.stripNo, 1);
  EXPECT_EQ(first.aget, 2);
  EXPECT_EQ(first.chGraw, 66);
  EXPECT_EQ(first.chGeom, 62);
  EXPECT_NEAR(first.cx, 53.25, 1e-9);
  EXPECT_NEAR(first.cz, -148.675, 1e-9);
  EXPECT_NEAR(first.x[0], 53.25, 1e-9);
  EXPECT_NEAR(first.z[0], -149.541, 1e-9);

  // 256 strips = 256 (aget, ch_graw) pairs (TODO/06).
  std::set<std::pair<int, int>> channels;
  std::set<std::pair<std::string, int>> strips;
  for (const Pad& pad : map.Pads()) {
    channels.insert({pad.aget, pad.chGraw});
    strips.insert({pad.stripDir, pad.stripNo});
  }
  EXPECT_EQ(channels.size(), 256u);
  EXPECT_EQ(strips.size(), 256u);
}

TEST(PadMapTest, EveryPadCentreFindsItsOwnPad) {
  const PadMap& map = Pads();
  std::size_t matched = 0;
  for (std::size_t i = 0; i < map.Pads().size(); ++i) {
    const Pad& pad = map.Pads()[i];
    if (map.Find(pad.cx, pad.cz) == static_cast<int>(i)) ++matched;
  }
  EXPECT_EQ(matched, map.Size());
}

TEST(PadMapTest, PointsOffTheBoardHaveNoPad) {
  const PadMap& map = Pads();
  EXPECT_EQ(map.Find(0., 0.), -1);          // beyond the far z edge
  EXPECT_EQ(map.Find(60., -200.), -1);      // beyond the x edge
  EXPECT_EQ(map.Find(0., -1000.), -1);      // far outside the grid
}

TEST(PadMapTest, PointJustInsideAVertexBelongsToThatPad) {
  const PadMap& map = Pads();
  for (int i : {0, 5000, 13009}) {
    const Pad& pad = map.Pads()[i];
    // 0.01 mm from vertex 1 towards the centre.
    const double dx = pad.cx - pad.x[1];
    const double dz = pad.cz - pad.z[1];
    const double norm = std::sqrt(dx * dx + dz * dz);
    const double px = pad.x[1] + 0.01 * dx / norm;
    const double pz = pad.z[1] + 0.01 * dz / norm;
    EXPECT_EQ(map.Find(px, pz), i) << "pad " << i;
  }
}

TEST(PadMapTest, MissingFileThrows) {
  EXPECT_THROW(PadMap("/no/such/pads.csv"), std::runtime_error);
}

namespace {

const char* const kHeader =
    "pad_id,strip_dir,strip_no,pad_no,aget,ch_graw,ch_geom,cx,cz,"
    "x0,z0,x1,z1,x2,z2,x3,z3";
const char* const kOnePad =
    "0,V,5,0,1,7,7,0,0,0,-0.866,0.5,0,0,0.866,-0.5,0";

// Writes a small CSV into the test's temporary directory and returns its path.
std::string WriteCsv(const std::string& name, const std::string& content) {
  const std::string path = std::string(testing::TempDir()) + "/" + name;
  std::ofstream out(path);
  out << content;
  out.close();
  return path;
}

}  // namespace

TEST(PadMapTest, CommentLinesAreIgnored) {
  const std::string path = WriteCsv(
      "pads_comments.csv",
      std::string("# a comment\n#\n") + kHeader + "\n" + kOnePad + "\n");
  const PadMap map(path);
  EXPECT_EQ(map.Size(), 1u);
  EXPECT_EQ(map.Pads()[0].stripDir, "V");
  EXPECT_EQ(map.Pads()[0].stripNo, 5);
  EXPECT_EQ(map.Find(0., 0.), 0);
}

TEST(PadMapTest, WrongColumnOrderThrows) {
  const std::string swapped =
      "pad_id,strip_no,strip_dir,pad_no,aget,ch_graw,ch_geom,cx,cz,"
      "x0,z0,x1,z1,x2,z2,x3,z3";
  const std::string path =
      WriteCsv("pads_swapped.csv", swapped + "\n" + kOnePad + "\n");
  EXPECT_THROW((PadMap(path)), std::runtime_error);

  const std::string missing =
      "pad_id,strip_dir,strip_no,pad_no,aget,ch_graw,ch_geom,cx,cz";
  const std::string shortPath =
      WriteCsv("pads_short.csv", missing + "\n0,V,5,0,1,7,7,0,0\n");
  EXPECT_THROW((PadMap(shortPath)), std::runtime_error);
}

// --- Stage 3 command line, output name and time bins -----------------------

namespace {

// getopt() wants a mutable argv; the literals are never written to.
bool Parse(const std::vector<std::string>& args, ChannelOptions& options,
           std::string& error) {
  std::vector<std::string> storage = args;
  std::vector<char*> argv;
  for (auto& arg : storage) argv.push_back(&arg[0]);
  argv.push_back(nullptr);
  return ParseChannelOptions(static_cast<int>(storage.size()), argv.data(),
                             options, error);
}

}  // namespace

TEST(ParseChannelOptionsTest, MandatoryOptionsOnly) {
  ChannelOptions options;
  std::string error;
  ASSERT_TRUE(Parse({"channel-response", "-i", "in.root", "-p", "pads.csv"},
                    options, error))
      << error;
  EXPECT_EQ(options.input, "in.root");
  EXPECT_EQ(options.pads, "pads.csv");
  EXPECT_DOUBLE_EQ(options.windowStartUs, 0.);
  EXPECT_LT(options.maxEvents, 0);
  EXPECT_FALSE(options.help);
}

TEST(ParseChannelOptionsTest, AllOptions) {
  ChannelOptions options;
  std::string error;
  ASSERT_TRUE(Parse({"channel-response", "-i", "a/in.root", "-p",
                     "geometry/pads.csv", "-w", "10.24", "-n", "3"},
                    options, error))
      << error;
  EXPECT_EQ(options.input, "a/in.root");
  EXPECT_EQ(options.pads, "geometry/pads.csv");
  EXPECT_DOUBLE_EQ(options.windowStartUs, 10.24);
  EXPECT_EQ(options.maxEvents, 3);
}

TEST(ParseChannelOptionsTest, MissingMandatoryOptionsFail) {
  ChannelOptions options;
  std::string error;
  EXPECT_FALSE(Parse({"channel-response"}, options, error));
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(Parse({"channel-response", "-i", "in.root"}, options, error));
  EXPECT_FALSE(Parse({"channel-response", "-p", "pads.csv"}, options, error));
}

TEST(ParseChannelOptionsTest, OutOfRangeValuesFail) {
  ChannelOptions options;
  std::string error;
  EXPECT_FALSE(Parse({"channel-response", "-i", "in.root", "-p", "pads.csv",
                      "-w", "nonsense"},
                     options, error));
  EXPECT_FALSE(Parse({"channel-response", "-i", "in.root", "-p", "pads.csv",
                      "-n", "0"},
                     options, error));
}

TEST(ParseChannelOptionsTest, NegativeWindowStartIsAllowed) {
  ChannelOptions options;
  std::string error;
  ASSERT_TRUE(Parse({"channel-response", "-i", "in.root", "-p", "pads.csv",
                     "-w", "-2.5"},
                    options, error))
      << error;
  EXPECT_DOUBLE_EQ(options.windowStartUs, -2.5);
}

TEST(ParseChannelOptionsTest, HelpNeedsNoOtherOption) {
  ChannelOptions options;
  std::string error;
  ASSERT_TRUE(Parse({"channel-response", "-h"}, options, error)) << error;
  EXPECT_TRUE(options.help);
}

TEST(ChannelUsageTest, MentionsTheMandatoryOptions) {
  const std::string usage = ChannelUsage();
  EXPECT_NE(usage.find("-i"), std::string::npos);
  EXPECT_NE(usage.find("-p"), std::string::npos);
}

TEST(ChannelOutputNameTest, InsertsReadoutBeforeTheSuffix) {
  EXPECT_EQ(ChannelOutputName("He_200mbar_0.3MeV_2000V.root"),
            "He_200mbar_0.3MeV_2000V_readout.root");
  EXPECT_EQ(ChannelOutputName("out/X_2000V.root"), "out/X_2000V_readout.root");
}

TEST(ChannelOutputNameTest, AppendsTheSuffixWhenTheInputHasNone) {
  EXPECT_EQ(ChannelOutputName("X_2000V"), "X_2000V_readout.root");
}

TEST(TimeBinTest, FortyNanosecondCells) {
  EXPECT_EQ(kBinNs, 40.);
  EXPECT_EQ(kNBins, 512);
  EXPECT_EQ(TimeBin(0., 0.), 0);
  EXPECT_EQ(TimeBin(39.999, 0.), 0);
  EXPECT_EQ(TimeBin(40., 0.), 1);
  EXPECT_EQ(TimeBin(14400., 0.), 360);  // 14.4 us drift
}

TEST(TimeBinTest, TimesOutsideTheWindowAreRejected) {
  EXPECT_EQ(TimeBin(-1., 0.), -1);
  EXPECT_EQ(TimeBin(20479.9, 0.), 511);  // 512 * 40 ns = 20.48 us
  EXPECT_EQ(TimeBin(20480., 0.), -1);
  EXPECT_EQ(TimeBin(15000., 30.), -1);  // window opens at 30 us
}

TEST(TimeBinTest, TheWindowStartIsInMicroseconds) {
  // 10.25 us is exact in binary; times right on a bin edge of a window start
  // that is not (10.24 us) are at the mercy of the rounding of w0 * 1000.
  EXPECT_EQ(TimeBin(10250., 10.25), 0);
  EXPECT_EQ(TimeBin(10290., 10.25), 1);
  EXPECT_EQ(TimeBin(10249., 10.25), -1);
  EXPECT_EQ(TimeBin(10260., 10.24), 0);
  EXPECT_EQ(TimeBin(10300., 10.24), 1);
  EXPECT_EQ(TimeBin(10230., 10.24), -1);
}
