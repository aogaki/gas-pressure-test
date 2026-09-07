# 10 レビュー指摘の修正 (発注書)

前提: TODO/09 (指摘 R1〜R21)。状態: 実装済み (2026-09-07)。全 121 テスト合格 (112 → +9: AT-1_after_init, AT-1_two_beamon, AT-1_backward_run/check, AT4-1_eventlist/_check, AT-S3_pads_run/check, AT-D2_summary)。R21 は現状維持と決定済みで作業なし。

検証結果 (発注者による):

- 統合後のクリーンビルドで 121/121 合格。at1_default の `.root` を消して AT-1_default_check だけ走らせると落ち、run から走らせ直すと通る (B1)
- R1/R2/R5 は新しい受け入れテスト (AT-1_after_init, AT-1_two_beamon, AT-1_backward) が固定した
- `drift_scan.sh` を scans/am241 相当の名前で dry run すると Stage 1 のファイルだけが並ぶ。dry run は logs/ も .failed も作らない

実装で決めたこと (発注書からの変更):

- 群 A: AT-1_backward と AT4-1_eventlist は run と check の 2 テストに分けた (このリポジトリの慣習)。AT-1_after_init と AT4-1_eventlist は自前で `rm -f ./*.root` する。RunAction.cc の `EndsWithRoot` は Geant4 側なので CliUtils.hh に移していない。hits の中点化の後、AT4-4 の makeTracks の平均飛跡長は 59.4 mm (期待 60.6 ±10 % の中)
- 群 B: `drift_scan.sh` は outdir に cd できなければ usage エラー (従来は mkdir で作っていた)。入力 0 本のメッセージは dry run でも出る。dry run の出力は名前を単一引用符で括る。`check_scan_summary.C` の contained_fraction は範囲でなく期待値との一致を判定する。AT-D2_dry のコマンド数は空白入りの名前を足したので 4
- 群 C: TODO/07 のイベント一覧の文は統合時に A4 の順序 (events → summary → waveforms) に合わせた

3 つの作業群に分け、別々の git worktree で並行して行う。各群は自分の worktree で全テストを通し、ブランチにコミットする。統合と最終検証は発注者 (Fable) が行う。

共通の規則:
- KISS。ここに書いていない設計変更はしない。迷ったら書いていないほうを選ばず、報告する
- TDD。テストを先に書いて失敗を見てから直す
- コードの識別子とコメントは英語。文書は日本語
- geometry/pads.csv、geometry/pads_d50.csv、external/tpcdaq-macros/channel_map.csv は git 外なので、worktree には元のツリーから手でコピーする (gitignore 済みなのでコミットには入らない)
- 全テスト (`cmake -S . -B build && cmake --build build -j && ctest --test-dir build -j4`) が合格すること。テスト数は群 A と B で増える

## 群 A: Stage 1 と Stage 4 のコード、KISS (R1, R2, R5, R6, R11, R15, R16, R18、コメント)

### A1. `/tpc/` コマンドを `/run/initialize` の前に限定する (R1, R2)

- src/TpcMessenger.cc: `DeclareMethod(...)` の戻り値に `.SetStates(G4State_PreInit)` を付ける (gas, pressure, hits の 3 つ)。ヘルプ文字列の "(before /run/initialize)" を hits にも付ける
- src/RunAction.cc `BeginOfRunAction`: 2 回目の呼び出し (`fNtuplesCreated` が既に true) なら `G4Exception("RunAction::BeginOfRunAction()", "tpc0003", FatalException, "one /run/beamOn per macro: ...")`。`fDetector.ApplyStepLimit()` の呼び出しは外す (hits は initialize 前に確定し、`Construct()` が既に呼ぶ)。RunAction から `DetectorConstruction&` の依存を外し、`ActionInitialization` の呼び出しも合わせる。`DetectorConstruction::ApplyStepLimit()` 自体は残す (単体テストが使う)
- 単体テスト (tests/unit/test_detector.cc): `G4StateManager::GetStateManager()->SetNewState(G4State_Idle)` にしてから `/tpc/gas He` を `ApplyCommand` すると戻り値が 0 でなく、`config.gas` が変わらないこと。テストの最後に `SetNewState(G4State_PreInit)` で戻す
- 受け入れテスト (tests/acceptance/CMakeLists.txt、AT-1 の節):
  - `AT-1_after_init`: マクロ at1_after_init.mac (`/tpc/gas Ar` → `/run/initialize` → `/tpc/gas He` → `/run/beamOn 10`) で終了コードが 0 でなく、`*.root` ができない
  - `AT-1_two_beamon`: マクロ at1_two_beamon.mac (`/run/beamOn 5` を 2 回) で終了コードが 0 でない

### A2. ガスに入らなかった α の記録 (R5)

- src/EventAction.cc `EndOfEventAction`: `fTrackLength == 0.` なら終点 = 一次頂点、tEnd = 0、eExit = e0、exited = 1 として書く (「入射点で全エネルギーのまま出た」)。コメントに理由 (線源は入射面上にあり、−z 向きはガスに入らない) を書く
- 受け入れテスト `AT-1_backward`: マクロ at1_backward.mac (Ar 200 mbar、`/gps/direction 0 0 -1`、10 イベント) → 既存の check_punchthrough.C を `check_punchthrough.C("Ar_200mbar_5.5MeV.root", 0, 0, -250, 1e-6, 5.5, 1e-6)` で流用 (trackLength が [0, 0]、zEnd = −250、eExit = 5.5、全 exited = 1)

### A3. hits の位置をステップの中点にする (R11)

- src/SteppingAction.cc: hits の x, y, z, t を pre と post の中点 (時刻も中点) にする。コメントに理由 (1 mm ステップの電離の代表点)
- 既存の AT-8 (時刻の単調増加)、AT-9 (箱の内側、1 mm 以下) はそのまま通ること。AT2-3、AT3-3 の期待値は変えない

### A4. Stage 4 のイベント一覧と逐次化 (R6, R18)

- src/aget_main.cc: イベント一覧は `events` (Stage 1 の全イベント) を最優先、無ければ `summary`、両方無ければ `waveforms` に現れたイベント。`waveforms` の全行を map に貯めるのをやめ、イベント一覧の順に 1 イベントずつ行を読み進めて書く (行は eventID 順という前提を関数コメントに明記。一覧に無い eventID の行は数えて最後に警告する)
- `-n` の意味は変えない (eventID < N)
- 受け入れテスト `AT4-1_eventlist` (pads.csv がある場合の節、AT4-3 の後): 新しい作業ディレクトリに s3_end2end の He_200mbar_0.3MeV_2000V.root をコピーし、`channel-response -n 3` → `aget-shaper -g 1000` → `check_raw.C(raw, readout, 10, 450, 6, 1000, 120, 223, 100, 5000)` で raw が 10 エントリ (events の数) であること。AT4-1_no_waveforms などは変えない

### A5. 重複と未使用の整理 (R15, R16)

- include/CliUtils.hh (ヘッダオンリー、inline): `ResetGetopt()`, `ToDouble(const char*, double&)`, `ToLong(const char*, long&)`, `EndsWith(const std::string&, const std::string&)`。src/DriftUtils.cc, src/ChannelUtils.cc, src/AgetUtils.cc の同名の無名名前空間関数を消してこれを使う。出力名の 3 関数は `EndsWith` を使う
- `DriftFormatNumber` を消し、`FormatNumber` (include/GasProperties.hh) を使う (src/DriftUtils.cc, src/Drifter.cc)
- `Drifter::TableWasGenerated()`, `Drifter::GasFile()`, `m_generated`, `m_gasFile` のうち使わなくなるものを消す (`m_gasFile` はコンストラクタで使うならローカル変数にする)
- tests/acceptance/check_drift.C の未使用の `status` (宣言と SetBranchAddress) を消す
- AGET の定数 `kNAget = 4`, `kNChannel = 68`, `kNCells = 512` を include/AgetResponse.hh に置き、src/aget_main.cc と tests/acceptance/check_raw.C のローカル定義を消す。`kBinNs` は include/ChannelUtils.hh のものを aget_main.cc が include して使う
- 単体テストは既存のものが全部通ること。CliUtils の関数は既存の CLI テストで間接的に検査される

### A6. コメントと期待値 (R14 の一部)

- src/PhysicsList.cc: 「δ 線は 10 m のカットで出ない (電子の閾値 172.6 keV ≫ T_max 3 keV)。蛍光 X 線が出ないのは option4 で PIXE が無効なため」に直す
- include/AgetResponse.hh: `kAgetCutoffTau` のコメントを「h(5τ) = −8.3e−4 (アンダーシュート側)」に直す
- tests/acceptance/check_tracks.C のコメントと tests/acceptance/CMakeLists.txt の AT4-4 の期待値を 60.6 mm (許容 ±10 %) にする

## 群 B: テスト基盤とスクリプト (R3, R4, R7, R8, R13, R19 の DEPENDS)

### B1. 前回の出力で合格しない (R3)

- tests/acceptance/CMakeLists.txt の `acceptance_run` を `sh -c 'rm -f ./*.root && exec "$0" "$1"' <exe> <macro>` の形にする
- 出力を作る他のテスト (AT-S2_run, AT2-4_cache, AT2-3_run, AT-M3_run, AT-D2_run, AT3-3_stage2, AT3-3_run, AT3-4_run, AT4-3_run, AT4-1_empty_run, AT4-4_stage2/stage3/run/uvw/tracks) は先頭で自分の出力ファイルを `rm -f` する。名前だけを見る `*_output_name` テストは、直前の run の掃除によって初めて意味を持つ
- 検証 (発注者が手で行う): run の出力を消して check だけ走らせると落ちること

### B2. 判定の余裕 (R7, R8)

- tests/acceptance/CMakeLists.txt の AT4-3 の `minAreaAdc` を 3000 → 5000
- tests/acceptance/check_raw.C の FPN 上限を `pedestal + 8 * noiseSigma` に (コメントに 10 イベントで 5 σ は失敗確率 3.5 % だったこと)
- tests/acceptance/check_energy.C の上限を `1.0 + 1e-9`
- `AT2-1_no_hits` に `DEPENDS AT2-4_cache` を足す (R19)

### B3. drift_scan.sh (R4, R13)

- 除外パターンを `*[0-9]V | *_readout | *_raw | *_uvw | *_tracks` にする。コメントも直す
- 「名前 電圧」を 1 行にして `set -- $1` で割るのをやめる。`xargs -P` に渡すのは name だけにし、電圧は外側のループで sh -c の引数として渡す (電圧ごとに xargs を回すか、`printf '%s\n'` の行を `name\tv` でなく 2 引数に) 。空白入りのファイル名で正しく動くこと
- 内側の `cd "$3" || exit 1` が失敗したときも `.failed` に名前を残す (outdir に cd できないときは `.failed` 自体が書けないので、その場合はスクリプトの先頭で `cd` 可能かを確かめて usage エラーにする、が最も単純)
- `-d` では `mkdir -p` と `rm -f .failed` をしない
- 受け入れテスト AT-D2_dry: `touch` に `..._1000V_raw.root`, `..._1000V_uvw.root`, `..._1000V_tracks.root` を足す (期待するコマンド数は 2 のまま)。空白入りの名前 `Ar 200mbar_5.5MeV.root` も置き、dry run の出力に `-i 'Ar 200mbar_5.5MeV.root'` 相当の 1 組 (2 電圧で 2 行) が含まれ、`-v` の直後が電圧であること。dry run 後に `logs/` ができていないこと
- scan.sh は変えない (dry run でもマクロを書くので mkdir が要る)

### B4. range_summary.C (R4)

- `ReadFile` で `electrons` か `waveforms` があるファイルを飛ばす (Stage 2/3 の出力)
- AT-S3: `range_summary.C(".", -143.9, 53.75)` を summary_pads.csv に出す run を足し、check_scan_summary.C に引数 (期待する contained_fraction 2 つ) を足して、既定では両行 1、パッド面では 0.3 MeV の行が 1、1.0 MeV の行が 0 であること
- atS2 の作業ディレクトリに Stage 2 の出力を置くテストは足さない (AT-D2 で十分)。代わりに B4 の単体としては、AT-S3 の run の前に `touch` でなく実物の Stage 2 出力をコピーするのは重いので、`range_summary.C` のスキップは AT-D2 の atD2 ディレクトリ (Stage 1 と Stage 2 の出力が両方ある) で `range_summary.C(".")` を走らせ、行数が 1 であることで検査する (`AT-D2_summary`)

## 群 C: 文書 (R9, R10, R12, R14 の残り)

- TODO/01: 「検出エリア」の文に「(2026-09-07 追記: TODO/06 以降、判定はパッド面 107.5 × 106.5 mm。`range_summary.C` の zMaxMm/xMaxMm)」。`/tpc/hits` の制約を「`/run/initialize` より前」に。`run` ntuple の説明を「1 マクロにつき `/run/beamOn` は 1 回。2 回目は FatalException で止まる」に。hits の x, y, z, t の説明を「ステップの中点」に。一次 α の既定の説明に「線源は入射面上にあるので、−z 向きの α はガスに入らず、exited = 1、eExit = e0、終点 = 入射点で記録される」
- TODO/03: `-f` の説明に「平均は不偏だが、セルあたりの分散は 1/f 倍になるので幅の比較には使えない」。status の説明に「−5 は読み出し面到達と側壁からの離脱の両方。区別は y 座標だけ」。W の説明に「Magboltz の W は電子用。α 用は He 42.7、CO2 34.2、Ar 26.4 eV (ICRU 31) で、He と CO2 の電子数は 3〜4 % 多い」
- TODO/05: 混合ガスの W/Fano が体積分率の線形平均であること (数式と He-90-CO2-10 の 40.47 の内訳)、エネルギー分配と Jesse 効果を含まず He 混合の電子数は下限であること。CO2 の密度: NIST 値 (実在気体、1 atm) の線形スケールは低圧で 0.68 % 過大、Magboltz は理想気体 (TODO/01 の密度の節にも 1 行)
- TODO/07: 1.166 → 1.1664。「実装で決めたこと」に AT4-4 と AT4-3 が `-g 1000` を固定していること (較正値と独立にシェーパーを検査するため) と、AT4-3 の `minAreaAdc = 5000` の 2 段判定
- TODO/08: 結果の節に「G = 3600 で V/W にも飽和セルがあり (300 イベントで V 18,443、W 18,807)、V+W の総電荷は G = 360 の 10 倍に対し 0.987 倍。G は約 +1 % のバイアス。U は 0.21 倍。実データ (先頭 200 イベント) は最大 ADC 4069 で飽和セル無し」。「V と W は G ≤ 3600 で比例」の文をこれに合わせる
- CLAUDE.md: 「(Stage 3〜4 のテストは登録されず警告が出る)」を「(pads.csv を使う Stage 3〜4 のテストは登録されず警告が出る。Stage 4 の CLI テストは走る)」に。規約に「`/tpc/` の 3 コマンドは `/run/initialize` より前、`/run/beamOn` は 1 マクロに 1 回」を足す。TODO の一覧に 09, 10 を足す
- TODO/09: 冒頭の状態を「TODO/10 で修正中」に

## 受け入れ (発注者が行う)

- 3 ブランチを main に統合し、クリーンビルドで全テスト合格 (増えたテストを含む)
- B1 の検証: AT-1_default の作業ディレクトリで `.root` を消して `AT-1_default_check` だけ走らせると落ちる
- R1/R2/R5 の再現マクロ (scratchpad) が期待どおり失敗、または正しい記録になる
- drift_scan.sh を scans/am241 相当のファイル名で dry run し、Stage 1 の 3 ファイルだけが並ぶ
