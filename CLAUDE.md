# gas-pressure-test

Geant4 + Garfield++ で mini TPC のガス圧を検討する小規模シミュレーション。設計は TODO/ が正 (plan → 01_architecture → 02_... → 09_review → 10_review_fixes の順に読む)。

## 進め方

- Fable が仕事を引き受け、設計と発注書 (TODO/NN_*.md) を書き、Opus / Sonnet のサブエージェントに実装を任せ、結果を Fable が検証する
- KISS が絶対のルール。小規模なので複雑な設計や過剰な最適化はしない
- TDD。テストを先に書き、通す最小限のコードを書く
- 受け入れテストは CTest から走らせる: `ctest --test-dir build -L acceptance`
- 会話とドキュメントは日本語。コードの識別子とコメントは英語

## 環境

- Geant4 11.3.2: /opt/Geant4 (`geant4-config` が PATH にある)
- ROOT 6.36.10: /opt/ROOT
- Garfield++ 2025.12: /opt/Garfield (`source /opt/Garfield/share/Garfield/setupGarfield.sh`)
- GoogleTest: homebrew (`find_package(GTest)`)
- pyROOT はこの Mac では壊れている。ROOT ファイルの検証は C++ マクロ (`root -l -b -q`) で行う

## 規約

- Stage 1 (Geant4) は UI マクロ駆動: `gas-pressure-test run.mac`。自前コマンドは `/tpc/gas`, `/tpc/pressure` (mbar), `/tpc/hits` の 3 つ
- Stage 2 (Garfield++) は CLI: `drift-electrons -i in.root -v 500`
- Stage 3 (ROOT のみ) は CLI: `channel-response -i in_500V.root -p geometry/pads.csv -w 20` (`-w` は窓の開始 [µs])。出力は `_readout.root`
- Stage 4 (ROOT のみ) は CLI: `aget-shaper -i in_500V_readout.root` (既定 `-t 223` ns, `-r 120` fC, `-g 3600`, `-p 450`, `-s 6`)。出力は `_raw.root` で、中身は実機と同じ `raw` ツリー。`external/tpcdaq-macros/` の解析マクロ (analyzeUVW.C → makeTracks.C) がそのまま走る
- `geometry/pads.csv` と `channel_map.csv` は実機ジオメトリ由来なので git に入れない。channel_map.csv は `scripts/make_channel_map.py geometry/pads.csv channel_map.csv` で pads.csv から作る。pads.csv が無い環境でもビルドと Stage 1〜2 のテストは通る (pads.csv を使う Stage 3〜4 のテストは登録されず警告が出る。Stage 4 の CLI テストは走る)
- `/tpc/` の 3 コマンドは `/run/initialize` より前、`/run/beamOn` は 1 マクロに 1 回
- ガスの指定は `名前-割合-名前-割合...` (体積パーセント、合計 100、最大 6 成分)。成分は He, Ar, CO2。単一ガスは `He` と書ける。例: `He-90-CO2-10`。この文字列をファイル名と `run.gas` にもそのまま使う (TODO/01 の「ガス」節)
- スキャン: `scripts/scan.sh` (Stage 1、`-H` で hits あり) → `scripts/drift_scan.sh` (Stage 2、電圧ごと)
- 単位: マクロは mbar、ROOT 出力は mm, ns, MeV
- 座標: z は α 線源軸 (入射面 z = -250 mm、ビーム軸ではない)、y 鉛直 (読み出し面 y = -100 mm)、x 水平。読み出し板の実寸と座標変換は TODO/06、パッド地図は geometry/pads.csv

## ビルド

```
cmake -S . -B build && cmake --build build -j && ctest --test-dir build
```
