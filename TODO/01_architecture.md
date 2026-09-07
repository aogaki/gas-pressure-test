# 01 全体設計と規約

plan の議論を受けて 2026-09-07 に決めたこと。以降の TODO はこの文書を前提にする。

## 目的

mini TPC に適したガス圧 (と将来的に電圧) を決めるためのデータを取る。一次粒子は α のみ。

- 第 1 段階: α の飛程がエネルギーとガス圧でどう変わるか (TODO/02)
- 第 2 段階: α が作った電離電子を電場でドリフトさせ、ガスボリューム底面に到達した位置と時刻、位置の広がりを得る (TODO/03)

想定する圧力の上限は 200 mbar。下限は未定。
He は 300 keV の α を検出エリア内で止めることが目的なので低圧側を使う。
検出エリアはガスボリューム全体 (200 mm × 200 mm × 500 mm)。「検出エリア内で止まる」は exited = 0 と同じ意味。

## パイプライン

| 段階 | 実行ファイル | ツール | 入力 | 出力 |
|---|---|---|---|---|
| Stage 1 | `gas-pressure-test` | Geant4 11.3.2 (/opt/Geant4) | UI マクロファイル 1 つ | `{gas}_{p}mbar_{E}MeV.root` |
| Stage 2 | `drift-electrons` | Garfield++ 2025.12 (/opt/Garfield) | Stage 1 の ROOT ファイル, `-v 電圧[V]` | `{gas}_{p}mbar_{E}MeV_{V}V.root` |
| Stage 3 | `channel-response` | ROOT のみ | Stage 2 の ROOT ファイル, `-p geometry/pads.csv` | `..._{V}V_readout.root` |
| Stage 4 | `aget-shaper` | ROOT のみ | Stage 3 の ROOT ファイル | `..._{V}V_raw.root` |

最初の 2 段階が物理 (TODO/02, TODO/03)、後の 2 段階が読み出し (TODO/06, TODO/07)。
Stage 3 は電子をパッド・ストリップ・チャンネルと 40 ns の時間ビンに振り分け、Stage 4 は
AGET の整形と ADC 変換を掛けて実機の生データと同じ `raw` ツリーを作る。Stage 4 の出力は
`external/tpcdaq-macros/` の実データ用マクロ (analyzeUVW.C → makeTracks.C) がそのまま読める。
読み出し板の地図 (`geometry/pads.csv`) と `channel_map.csv` は実機ジオメトリ由来なので
git に入れない (TODO/07)。無い環境では Stage 3〜4 のテストは登録されない。

役割分担の理由:

- Geant4 は α の阻止能 (NIST ASTAR ベース) と多重散乱を持つが、eV 領域の電子をガス中で輸送する物理を持たない。
- Garfield++ は Magboltz による電子輸送 (ドリフト速度・拡散) を持つが、低エネルギー α の阻止能は持たない (Heed は高速粒子向け)。
- 境界を ROOT ファイルにすると、電圧を変えるたびに Stage 1 を再実行する必要がなく、両段階を独立にテストできる。
- Stage 1 は電圧を使わない。数百 V の電場は MeV の α に影響しないため。

## 設計判断: Stage 1 はマクロ駆動

plan では CLI オプション (`-g -p -e -m`) が案として挙がっていたが、Geant4 の UI マクロに一本化する。

```
gas-pressure-test run.mac
```

理由:

- 一次粒子の設定は G4GeneralParticleSource (GPS) に任せる。角度分布、位置分布、エネルギー分布はすべて `/gps/` コマンドで指定でき、コードを書かなくてよい。将来の「角度を持った α」がそのまま扱える
- 乱数シードは `/random/setSeeds`、イベント数は `/run/beamOn`、出力名は `/analysis/setFileName` と、Geant4 組み込みコマンドで済む。自前で持つのはガス・圧力・hits の 3 コマンドだけ
- CLI とマクロを併用すると優先順位や命名規則が増えて KISS に反する
- マクロファイルがそのまま実行記録になる。スキャンはテンプレートを sed で書き換えて生成する

Stage 2 は Geant4 ではないので普通の CLI (`-i 入力 -v 電圧`) にする。

### マクロで使うコマンド

| コマンド | 引数 | 既定値 | 制約 |
|---|---|---|---|
| `/tpc/gas` | ガス指定 (下の「ガス」節) | Ar | `/run/initialize` より前 |
| `/tpc/pressure` | 数値 [mbar] (単位は付けない) | 1013.25 | `/run/initialize` より前 |
| `/tpc/hits` | true, false | false | `/run/beamOn` より前 |
| `/analysis/setFileName` | `.root` まで書いた名前 | 自動命名 | Geant4 組み込み。名前にドットがあると Geant4 が最後のドット以降を拡張子と誤認して落ちるので `.root` を付ける。付け忘れはアプリが補う |
| `/gps/...` | GPS の全コマンド | alpha, 5.5 MeV, (0, 0, -250 mm), 方向 +z | Geant4 組み込み |
| `/random/setSeeds` | 整数 2 つ | Geant4 既定 | 再現性が要るなら必ず書く |
| `/run/initialize`, `/run/beamOn N` | | | |

- 未知のガス名や 0 以下の圧力は G4Exception (FatalException) で終了する
- 自動命名: `/analysis/setFileName` が無ければ BeginOfRun で `{gas}_{p}mbar_{E}MeV.root` にする。E は GPS のエネルギー分布が Mono のときはその値 [MeV]、それ以外は分布名 (例: `He_200mbar_Lin.root`)。数値は `%g` 相当の最短表記
- 自動命名でも `.root` を付けた名前を G4AnalysisManager に渡す (同じ理由)
- `/tpc/hits true` のときだけ `hits` ntuple を作り、ガス内の最大ステップ長を 1 mm に制限する。false のときはステップ制限なしで速く走る。1M イベントのスキャンで hits を書くと数十 GB になるため既定は false

マクロの例:

```
# run_He_200mbar_0.3MeV.mac
/tpc/gas He
/tpc/pressure 200
/tpc/hits false
/run/initialize
/gps/energy 0.3 MeV
/random/setSeeds 1 1
/run/beamOn 1000000
```

### 並列実行

MT は使わない。プロセスは互いに独立なので、マクロを変えて複数同時に走らせてよい。同じガス・圧力・エネルギーを同時に走らせるときはシードと出力名 (または作業ディレクトリ) を変える。

## 幾何と座標系

現状は単純化したガスボリュームだけ。実機の容器、フィールドケージ、GEM スタック、線源周りの構造物は入っていない。将来 CAD ファイルから本気のジオメトリを作る (TODO/08 の方針)。そのときの前提:

- この Geant4 は GDML 無しでビルドされている (`geant4-config --features` で gdml[no])。GDML を使うなら xerces-c を入れて Geant4 を再ビルドする。再ビルドせずに済む道は CADMesh (STL/PLY/OBJ を G4TessellatedSolid として読む) で、CAD から STL を書き出せばよい
- 実機の電場 (フィールドケージ、GEM 周り) は一様ではないので、Stage 2 の ComponentConstant を電場マップ (ANSYS/COMSOL/Elmer の出力を Garfield++ が読む) か neBEM に置き換える
- 線源ホルダーとコリメータの形状も CAD から入れ、線源の角度広がりはモデルでなく幾何で決まるようにする

- world: 1 m 立方の真空 (G4_Galactic)
- ガスボリューム: 200 mm × 200 mm × 500 mm の直方体。中心を原点に置く
- z: α 線源軸。入射面は z = -250 mm、α は +z に進む。実機の基板図では −y_det に相当し、加速器のビーム軸 (+x_det) は sim の +x に対応する (TODO/06)。「ビーム軸」とは呼ばない
- y: 鉛直 = ドリフト方向。読み出し面 (底) は y = -100 mm、カソードは y = +100 mm。ドリフト長は最大 200 mm
- x: 水平
- 一次 α の既定: 位置 (0, 0, -250 mm)、方向 +z、t = 0。マクロの `/gps/` で変更できる

## 単位

- マクロ: 圧力は mbar (単位なし)、エネルギーは GPS の書式 (`0.3 MeV`)
- Stage 2 CLI: 電圧 V
- ROOT 出力: Geant4 の内部単位のまま。長さ mm、時間 ns、エネルギー MeV
- 温度は 20 °C (293.15 K) 固定
- 密度: ρ(P) = ρ_NIST × P / 1013.25 mbar (理想気体)。Geant4 の NIST ガス密度は 20 °C, 1 atm の値

## ガス

成分は次の 3 つ。

| 成分名 | Geant4 NIST 名 | Magboltz 名 | ρ_NIST [g/cm3] |
|---|---|---|---|
| He | G4_He | he | 1.66322e-4 |
| Ar | G4_Ar | ar | 1.66201e-3 |
| CO2 | G4_CARBON_DIOXIDE | co2 | 1.84212e-3 |

### ガス指定の書式 (TODO/05)

`名前-割合-名前-割合...`。例: `He-90-CO2-10`, `Ar-90-CO2-10`, `He-92.5-CO2-7.5`。

- 割合は体積 (モル) パーセント。合計は 100、各割合は 0 より大きい。小数も可
- 成分は 1 から 6 個 (Magboltz の上限)。単一ガスは `He` と書ける (`He-100` と同じ)
- 順序はそのまま名前に使う (正規化しない)
- この文字列を `/tpc/gas`、ファイル名、`run.gas`、Stage 2 の組成、ガステーブルのキャッシュ名すべてで使う。区切りにコロンやスラッシュを使わないのは、ROOT のファイル名と Unix のパスで安全にするため
- 解析は `ParseGasSpec`(`include/GasProperties.hh`) 1 か所。Geant4 非依存なので Stage 1 (gpt_core) と Stage 2 (drift_util) の両方でコンパイルする

### 密度と材料

- 密度は理想気体の分圧の和: ρ(P) = (P / 1013.25) × Σ (f_i / 100) ρ_i,NIST
- 単一ガスは `G4NistManager::BuildMaterialWithNewDensity` で NIST 材料を base material にして作る。α の阻止能モデル (G4BraggIonModel) は ASTAR データを材料の同一性で引くため、base material 経由で参照されることを受け入れテストで確認する
- 混合ガスは `new G4Material(name, density, ncomponents, kStateGas, 293.15 K, P)` に NIST 材料を `AddMaterial(nist_i, w_i)` で加える。質量分率は w_i = f_i ρ_i,NIST / Σ f_j ρ_j,NIST (理想気体なので分子量の表は不要)。base material が無いので阻止能は Geant4 の元素ベースの計算になる。ASTAR の飛程加法則 1/R = Σ w_i / R_i と 5.5 MeV で 1〜3 % 一致する (AT-M2)
- 材料名はどちらも `{gas}_{p}mbar`。同じ名前で 2 回作らない

## ROOT 出力の構造 (Stage 1)

G4AnalysisManager の ROOT 形式で書く。ROOT ライブラリは Stage 1 ではリンクしない。

ntuple `events` (1 行 = 1 イベント)

| 列 | 型 | 意味 |
|---|---|---|
| eventID | I | イベント番号 |
| e0 | D | 一次 α の運動エネルギー [MeV] |
| x0, y0, z0 | D | 一次 α の生成位置 [mm] |
| dx0, dy0, dz0 | D | 一次 α の初期方向 (単位ベクトル) |
| trackLength | D | ガス内での α の経路長 [mm] |
| xEnd, yEnd, zEnd | D | α の停止点、または突き抜けた場合はガス出射点 [mm] |
| tEnd | D | その時刻 [ns] |
| eExit | D | 出射時の運動エネルギー [MeV]。停止なら 0 |
| exited | I | 突き抜けたら 1、停止なら 0 |
| edepTotal | D | ガス内の全エネルギー付与の合計 [MeV] (二次粒子を含む) |

一次粒子の情報は G4Event の primary vertex から取る。GPS で分布を与えた場合も、実際に生成された値が記録される。

ntuple `hits` (1 行 = ガス内でエネルギー付与のあった 1 ステップ。`/tpc/hits true` のときのみ)

| 列 | 型 | 意味 |
|---|---|---|
| eventID | I | イベント番号 |
| trackID | I | Geant4 の track ID (一次 α は 1) |
| pdg | I | PDG コード。α = 1000020040、e- = 11 |
| x, y, z | D | ステップ終点 [mm] |
| t | D | ステップ終点の global time [ns] |
| edep | D | このステップのエネルギー付与 [MeV] |
| stepLength | D | ステップ長 [mm] |

- Stage 2 は `hits` の edep を W 値で電子数に変換して使う

ntuple `run` (1 行だけ。Stage 2 がガスと圧力を知るため)

| 列 | 型 | 意味 |
|---|---|---|
| gas | S | `/tpc/gas` の値 |
| pressure | D | `/tpc/pressure` の値 [mbar] |
| hits | I | `/tpc/hits` の値 (0/1) |

`run` は `/run/beamOn` ごとに 1 行書かれる。1 マクロにつき `/run/beamOn` は 1 回だけにする (複数回書くと `events` も混ざる)。

## 物理

- G4EmStandardPhysics_option4 + G4StepLimiterPhysics のみ。ハドロン物理と崩壊は入れない (数 MeV α の核反応は無視できる)
- 生成閾値 (production cut) は 10 m。δ 線と蛍光 X 線を独立した飛跡として作らず、α のエネルギー付与をそのステップに留める。5.5 MeV の α が作る δ 線は 3 keV 以下で、200 mbar でも飛程 1 mm 未満なので 1 mm 刻みの hits には影響しない。検証で hits の行の 85 % が δ 線 (付与エネルギーは 9 %) だったため、ファイルサイズと速度のためにこうした
- Serial run manager。同じマクロ (同じシード) なら同じ結果になること

## 開発の進め方

- 役割分担: Fable が仕事を引き受け、設計と発注書 (TODO/NN_*.md) を書き、Opus / Sonnet のサブエージェントに実装を任せ、結果を Fable が検証する
- KISS と TDD (plan の通り)
- C++17、CMake、GoogleTest (homebrew)、CTest
- テストは 3 層
  1. 純関数の単体テスト (Geant4 非依存): ガス名、密度計算、ファイル名
  2. Geant4 依存の単体テスト: 材料、幾何
  3. 受け入れテスト: 実行ファイルをマクロで走らせ、ROOT マクロで出力を検証 (CTest から実行)
- pyROOT はこの Mac では壊れているので使わない。ROOT の検証は C++ マクロで行う
- ディレクトリ構成

```
gas-pressure-test/
  CLAUDE.md           進め方と環境の要約
  CMakeLists.txt
  include/            ヘッダ
  src/                実装と main
  macros/             実行用マクロの例・テンプレート
  tests/unit/         GoogleTest
  tests/acceptance/   受け入れテストのマクロ、ROOT マクロ、CTest 登録
  reference/astar/    NIST ASTAR の参照データ
  TODO/               設計・タスク
```

## 参照データ

`reference/astar/` に NIST ASTAR (2026-09-07 取得) の α 阻止能・飛程表 (He, Ar, CO2)。受け入れテストの期待値はここから計算する。
