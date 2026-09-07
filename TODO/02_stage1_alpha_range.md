# 02 Stage 1: α の飛程 (Geant4)

前提: TODO/01_architecture.md

## ゴール

```
./gas-pressure-test macros/run_He_200mbar_0.3MeV.mac
```

で `He_200mbar_0.3MeV.root` ができ、`events` ntuple から飛程分布が、(`/tpc/hits true` なら) `hits` ntuple からエネルギー付与の分布が得られる。

## 起動とマクロの仕様

```
gas-pressure-test <macro file>
```

- 引数が 1 つでなければ usage を stderr に出して終了コード 1
- マクロファイルが開けなければ終了コード 1
- マクロ内のコマンドは TODO/01 の表の通り。`/tpc/gas` と `/tpc/pressure` は `/run/initialize` より前に書く
- 未知のガス名、0 以下の圧力は G4Exception (FatalException) で終了コード 0 以外
- 出力はカレントディレクトリ。既存ファイルは黙って上書き
- 自動命名の数値は `%g` 相当の最短表記: 200 → `200`、0.3 → `0.3`、1013.25 → `1013.25`

## 期待値の根拠

NIST ASTAR の CSDA 飛程 RANGE(c) [g/cm2] を ρ(P) = ρ_NIST × P/1013.25 で割る。
投影飛程 (zEnd - z0) は RANGE(p) と比べる。データは `reference/astar/`。

| ガス | E [MeV] | P [mbar] | ρ [g/cm3] | RANGE(c) [g/cm2] | 期待経路長 [mm] | RANGE(p) [g/cm2] | 期待投影飛程 [mm] |
|---|---|---|---|---|---|---|---|
| Ar | 5.5 | 1013.25 | 1.66201e-3 | 7.508e-3 | 45.2 | 7.358e-3 | 44.3 |
| He | 5.5 | 1013.25 | 1.66322e-4 | 3.923e-3 | 235.9 | 3.912e-3 | 235.2 |
| CO2 | 5.5 | 1013.25 | 1.84212e-3 | 5.068e-3 | 27.5 | 5.029e-3 | 27.3 |
| Ar | 5.5 | 200 | 3.2806e-4 | 7.508e-3 | 228.9 | 7.358e-3 | 224.3 |
| CO2 | 5.5 | 200 | 3.6361e-4 | 5.068e-3 | 139.4 | 5.029e-3 | 138.3 |
| He | 0.3 | 200 | 3.2829e-5 | 2.433e-4 | 74.1 | 2.340e-4 | 71.3 |
| Ar | 0.3 | 200 | 3.2806e-4 | 5.469e-4 | 16.7 | 4.243e-4 | 12.9 |
| CO2 | 0.3 | 200 | 3.6361e-4 | 3.319e-4 | 9.13 | 2.992e-4 | 8.23 |
| He | 5.5 | 200 | 3.2829e-5 | 3.923e-3 | 1195 (突き抜け) | - | - |

参考: He 中の 300 keV α の CSDA 飛程 [mm]

| P [mbar] | 1013.25 | 200 | 100 | 50 | 30 | 20 |
|---|---|---|---|---|---|---|
| 飛程 | 14.6 | 74.1 | 148 | 296 | 494 | 741 |

## 受け入れテスト

各テストは専用のマクロ (`tests/acceptance/macros/`) で 1000 イベント、`/random/setSeeds 1 1` で実行し、ROOT マクロ (`tests/acceptance/*.C`) で判定する。CTest に `acceptance` ラベルで登録し `ctest -L acceptance` で全部走る。判定マクロは失敗時に終了コード 0 以外で終わる。

### AT-1 起動とファイル名

- 10 イベントのマクロ (Ar, 200 mbar, 5.5 MeV, hits false) → `Ar_200mbar_5.5MeV.root` が生成され、`events` に 10 エントリ、`hits` ntuple は存在しない
- 同じマクロで `/tpc/hits true` → `hits` に 1 エントリ以上
- `/analysis/setFileName custom` を書いたマクロ → `custom.root` ができる
- `/analysis/setFileName He_200mbar_0.3MeV` (ドット入り、拡張子なし) を書いたマクロ → `He_200mbar_0.3MeV.root` ができ、`events` に 10 エントリ
- どのファイルにも `run` ntuple が 1 エントリあり、gas, pressure, hits がマクロの値と一致する
- 引数なしで起動 → 終了コード 1、ファイルは作られない
- `/tpc/gas Xe` を書いたマクロ → 終了コード 0 以外

### AT-2 飛程 (1013.25 mbar, 5.5 MeV)

Ar, He, CO2 それぞれで

- 全イベント exited = 0
- trackLength の平均が期待経路長の ±3 % 以内
- (zEnd - z0) の平均が期待投影飛程の ±3 % 以内
- trackLength の標準偏差が平均の 0.5 % 以上 3 % 以下 (飛程ストラグリングが出ていること)

### AT-3 圧力スケーリング (200 mbar, 5.5 MeV)

Ar と CO2 で AT-2 と同じ判定。期待値は表の 200 mbar の行。

### AT-4 低エネルギー (200 mbar, 0.3 MeV)

He, Ar, CO2 で

- 全イベント exited = 0
- trackLength の平均が期待経路長の ±5 % 以内
- (zEnd - z0) の平均が期待投影飛程の ±10 % 以内 (Ar は detour factor 0.78 で多重散乱が大きい)

### AT-5 突き抜け (He, 200 mbar, 5.5 MeV)

- 全イベント exited = 1
- trackLength が 500 mm 以上 505 mm 以下
- zEnd が 250 mm (誤差 0.01 mm 以内)
- eExit の平均が 3.84 MeV の ±5 % 以内 (ASTAR で 500 mm 分の質量厚 1.64e-3 g/cm2 を差し引いた残留飛程から内挿)

### AT-6 エネルギー保存 (Ar, 1013.25 mbar, 5.5 MeV, hits true)

- 各イベントで `hits` の edep の合計が events.edepTotal と一致 (1e-6 MeV 以内)
- edepTotal の平均が e0 の 99.9 % 以上 100.0 % 以下 (生成閾値 10 m なので δ 線も蛍光 X 線も脱出しない)

### AT-7 再現性

- 同じマクロで 2 回実行し (出力名は `/analysis/setFileName` で変える)、`events` の全列が全エントリで一致する
- `/random/setSeeds` を変えると trackLength の並びが変わる

### AT-8 時間 (Ar, 1013.25 mbar, 5.5 MeV, hits true)

- 一次 α (trackID = 1) の `hits` の t はイベント内で単調非減少
- tEnd の平均が 2 ns 以上 20 ns 以下 (v0 = 1.6 cm/ns、減速しながら 4.5 cm)

### AT-9 ヒットの健全性 (Ar, 200 mbar, 5.5 MeV, hits true)

- 全ヒットで stepLength ≤ 1.0 mm (誤差 1e-6 mm)
- 全ヒットで |x| ≤ 100、|y| ≤ 100、|z| ≤ 250 mm (誤差 1e-6 mm)
- 全ヒットで edep > 0

### AT-10 hits の有無で物理が変わらないこと (Ar, 200 mbar, 5.5 MeV)

- hits false と hits true で trackLength の平均が互いに 1 % 以内

### AT-11 GPS の分布が記録されること (Ar, 200 mbar, 100 イベント)

- `/gps/ene/type Lin`, `/gps/ene/min 1 MeV`, `/gps/ene/max 6 MeV`, `/gps/ene/gradient 0`, `/gps/ene/intercept 1` → e0 の最小が 1 MeV 以上、最大が 6 MeV 以下、標準偏差が 1 MeV 以上。自動命名は `Ar_200mbar_Lin.root`
- `/gps/ang/type iso` に `/gps/ang/mintheta 170 deg`, `/gps/ang/maxtheta 180 deg` → dz0 が 0.98 以上、dx0 の標準偏差が 0 より大きい
  - 符号は実測で確認した。GPS の iso が作る運動量は theta の逆向きで、theta = 0 deg なら dz0 = -1、theta = 180 deg なら dz0 = +1 になる。したがって +z に飛ばすには theta を 180 deg 付近にする
  - theta 170-180 deg のとき dz0 は cos(10 deg) = 0.9848 以上。実測は 0.9849 から 0.9999

## 単体テスト (GoogleTest, `tests/unit/`)

- `GasNistName("CO2")` == `"G4_CARBON_DIOXIDE"`、`"He"`, `"Ar"` も同様。未知のガスは `std::invalid_argument`
- `GasDensity("He", 200)` == 3.2829e-5 g/cm3 (相対誤差 1e-4)。1013.25 mbar で NIST 値に一致。0 以下の圧力は `std::invalid_argument`
- `FormatNumber(200)` == `"200"`、`FormatNumber(0.3)` == `"0.3"`、`FormatNumber(1013.25)` == `"1013.25"`
- `OutputFileName("He", 200, "0.3")` == `"He_200mbar_0.3MeV"` (拡張子は G4AnalysisManager が付ける)
- (Geant4 依存) `BuildGasMaterial("Ar", 200)` の密度が 3.2806e-4 g/cm3、`GetBaseMaterial()` の名前が `G4_Ar`、状態が kStateGas。同じ引数で 2 回呼んでも同じポインタ (G4 の材料テーブルは名前の重複を許さない)

## 実装タスク (TDD の順)

1. CMake の骨組み。Geant4 と GTest を find_package し、失敗する単体テストが 1 つ走る
2. `GasNistName`, `GasDensity`, `FormatNumber`, `OutputFileName` (純関数、Geant4 非依存)
3. `BuildGasMaterial` (G4NistManager)
4. `DetectorConstruction`: world + ガス直方体 + G4UserLimits。ガス名と圧力は外から与える
5. `TpcMessenger` (G4GenericMessenger): `/tpc/gas`, `/tpc/pressure`, `/tpc/hits`
6. `PhysicsList`: G4EmStandardPhysics_option4 + G4StepLimiterPhysics
7. `PrimaryGeneratorAction`: GPS と既定値
8. Action 群と G4AnalysisManager: `events` と (hits true なら) `hits`、自動命名
9. `main`: Serial run manager、引数チェック、マクロ実行
10. 受け入れテスト AT-1 から AT-11 を CTest に登録し、全部合格させる
11. 速度計測: Ar 200 mbar 5.5 MeV を 10000 イベント、hits false と true で走らせ、イベント/秒とファイルサイズをこの文書の「計測結果」に記録する

## 計測結果

2026-09-07、Apple Silicon (Darwin 25.6.0)、Apple clang、`CMAKE_BUILD_TYPE=Release` (-O3)、
Serial run manager。Ar 200 mbar、5.5 MeV、10000 イベント、`/random/setSeeds 1 1`。
実時間はプロセス全体 (初期化は約 0.4 s)。

| `/tpc/hits` | 実時間 [s] | イベント/秒 | 出力 [MB] | 1 イベントあたり |
|---|---|---|---|---|
| false | 20.5 | 488 | 0.66 | 66 B |
| true | 61.7 | 162 | 1826 | 183 kB |

- hits true の 1 イベントあたりのヒット数は約 3200 行 (α 本体の 1 mm 刻み + δ 線)
- 1M イベントに外挿すると hits false で約 34 分・66 MB、hits true で約 1.7 時間・183 GB
- 受け入れテストの `build/tests/acceptance/` は hits true の 3 ラン (各 1000 イベント) で約 600 MB を使う

## 検証後の追加タスク (2026-09-07)

検証で分かったこと: 46 テスト全て合格、飛程は ASTAR と 1.5 % 以内で一致。hits true の 1 イベント 3200 行のうち 85 % は δ 線 (pdg 11) の行で、付与エネルギーの 9 % しか持たない。`/analysis/setFileName` にドット入りの名前を渡すと Geant4 が拡張子と誤認して SIGSEGV する。

1. 出力名の補正: BeginOfRunAction で G4AnalysisManager のファイル名が `.root` で終わらなければ `.root` を付けて設定し直す (自動命名も同じ経路にする)。AT-1 にドット入り名のケースを追加。Geant4 の内部状態が回復せず落ちる場合はその旨を報告し、TODO/01 の「付け忘れはアプリが補う」を消す
2. 生成閾値を 10 m にする (PhysicsList の defaultCutValue)。`G4StepLimiterPhysics::SetApplyToAll(true)` は不要になるので消す。AT-6 の期待値を 99.9 % に変更。全テストが通ることを確認
3. `run` ntuple (gas S, pressure D, hits I) を 1 行書く。TODO/01 の表の通り。AT-1 の判定に追加
4. 速度計測 (タスク 11) をやり直し、「計測結果」に 2 つ目の表として追記する (生成閾値変更後)。hits の行数の内訳 (pdg ごと) も書く
5. 圧縮: G4AnalysisManager の既定 (zlib レベル 1) のまま。`/analysis/compression` で変えられることを macros/ の例にコメントで書く

## エネルギースキャンの計画 (要相談)

0.3 MeV から 10 MeV を 0.1 MeV 刻み (98 点)、1 ラン 1M イベントの案について。

- hits false の実測は 488 イベント/秒 (Ar 200 mbar 5.5 MeV、生成閾値変更前)。1M イベントで約 34 分、66 MB。98 点 × 3 ガス × 5 圧力 = 1470 ランなら 14 プロセス並列で約 60 時間。現実的だが長い
- hits true で 1M イベントは 1 ファイル数十 GB になるので、hits はドリフトに使う少数のランに限る
- 代案: GPS の一様スペクトル (`/gps/ene/type Lin`, gradient 0) で 0.3 から 10 MeV を 1 ランで走らせ、e0 でビン分けする。1M イベントなら 0.1 MeV あたり約 1 万イベント。3 ガス × 5 圧力 = 15 ランで 14 プロセス並列なら約 1 時間。コードは不要 (AT-11 で動作確認済み)

## 実装メモ

- 一次 α の生成点はガス入射面の上。Geant4 は面上の点をガス側として扱うが、警告が出る場合は z0 を -250 mm + 1e-6 mm にずらす
- `exited` は α のステップの post-step point がガスから world に出たとき (fGeomBoundary) に立てる
- trackLength は α のガス内ステップのステップ長の和
- `/tpc/hits` は G4UserLimits の SetMaxAllowedStep を 1 mm と DBL_MAX で切り替える。G4StepLimiterPhysics は常に登録する
- 自動命名は BeginOfRunAction で `G4AnalysisManager::GetFileName()` が空のときだけ行う。名前は `.root` 付きで渡す。GPS のエネルギーは `G4GeneralParticleSource::GetCurrentSource()->GetEneDist()` の `GetEnergyDisType()` と `GetMonoEnergy()` から取る
- Serial なので `SetNtupleMerging` は不要
