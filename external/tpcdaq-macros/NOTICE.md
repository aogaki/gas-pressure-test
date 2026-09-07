# NOTICE

These files were copied verbatim on 2026-09-07 from the mini-eTPC DAQ project
tpcdaq-rs (macros/ of /Users/aogaki/Workspace/tpcdaq-rs, author: the same people as this
repository) so that the simulated raw files can be analysed with the exact code used
for real data (TODO/07, AT4-4). Keep them in sync by copying again; do not edit here.

channel_map.csv and geometry/pads.csv are derived from the real detector geometry
(TPCReco geometry .dat) and are NOT committed: they are generated locally with
make_pads.py (pads.csv) and scripts/make_channel_map.py (channel_map.csv from pads.csv).

- README.md  md5 7a1afd2b35f41ced5369321e93c91c4b
- graw2root.C  md5 be6060cc4a871e2849167066cbe42519
- analyzeUVW.C  md5 b015ee494d8163ceb548fb755b3ed845
- makeTracks.C  md5 5826a8fe137c78e64a5b4ce0f574d7f6
- analyzeTracks.C  md5 1bfafb4e4390b0ce3c137ac603b8282f
- make_pads.py  md5 c45e4eedf3ac6532baa33f5d27eeb89f
