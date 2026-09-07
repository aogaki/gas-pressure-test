# 09 批判的レビュー (2026-09-07)

前提: TODO/01〜08。状態: TODO/10 で修正済み (2026-09-07)。R21 は現状維持。

## 進め方

- Fable が全コード (約 6,400 行: include/, src/, scripts/, tests/, CMake, TODO) を読み、疑った点を scratchpad で実験して確認した
- 並行して 3 名に発注: Opus (物理と正当性)、Opus (テストとスクリプトの健全性)、Sonnet (KISS と文書整合)。いずれも読み取り専用で、実験は scratchpad で行った
- 各指摘は Fable が裏取りした。印: **✔** = Fable が自分で再現、**○** = 担当の実験結果を証拠つきで採用 (再現はしていない)。裏取りで否定した主張は「却下した指摘」に残す

## 結論

物理の中核は正しい。飛程 (ASTAR)、ドリフト (速度、拡散、到達時刻)、整形関数、パッド地図、単位換算、乱数の再現性は、受け入れテストと今回の実験の両方で裏付けられた。

直すべきものは 3 群ある。

1. **Stage 1 の状態管理**: `/tpc/` コマンドを `/run/initialize` の後に呼ぶと黙って壊れる (R1, R2)。修正は数行
2. **テスト基盤**: 受け入れテストが前回の出力で合格しうる (R3)、固定シードで辛うじて通る判定がある (R7, R8)
3. **スクリプトの入力選別と引数の受け渡し** (R4, R13)

物理の系統誤差 (R9〜R12) はコードでなく文書に残す。KISS の指摘 (R15〜R18) は機能に影響しない。

## 指摘

### 高: 黙って間違う、またはテストが偽陰性になる

**R1 ✔ `/tpc/gas` `/tpc/pressure` を `/run/initialize` の後に呼ぶと、材料は古いまま `run` ntuple に新しい値が書かれる**
- 場所: src/TpcMessenger.cc (ガード無し)、src/DetectorConstruction.cc (`Construct()` は 1 回だけ)、src/RunAction.cc `WriteRunNtuple()` (`fConfig` をそのまま書く)
- 実験: Ar 200 mbar で初期化した後に `/tpc/gas He` `/tpc/pressure 50` → 終了コード 0、`run.gas = He, pressure = 50`、平均 trackLength 228.6 mm (Ar 200 mbar の値。He 50 mbar なら約 950 mm)。Stage 2 は `run` を信じて He の Magboltz を走らせるので、以後の全段が誤った条件で走る
- 修正案: `G4GenericMessenger::DeclareMethod(...).SetStates(G4State_PreInit)` を 3 コマンドに付ける。以後は "illegal application state" でバッチが止まり、`main` が終了コード 1 を返す。受け入れテストを 1 本足す (`! gas-pressure-test at1_after_init.mac`)。単体テスト `TpcMessengerTest` は PreInit 状態で走るので影響なし

**R2 ✔ `/tpc/hits` を `/run/beamOn` の間で切り替えると `run` ntuple が壊れる。同じマクロの 2 回目の `/run/beamOn` は 1 回目のファイルを上書きする**
- 場所: include/Ntuples.hh (`kHits = 1` 固定)、src/RunAction.cc (`fNtuplesCreated` で 1 回しか作らない、毎ラン `OpenFile()`)、src/SteppingAction.cc
- 実験: `hits false` で 5 イベント → `/tpc/hits true` → 5 イベント: `run` が 1185 行 (ゴミ行は pressure = 0, hits = 1000020040)、Analysis_W001 の警告 2368 件、終了コード 0。`events` は 2 回目の 5 イベントだけ (1 回目は RECREATE で消えた)。eventID も 0 から振り直される
- 修正案: `/tpc/hits` も PreInit 限定にする (TODO/01 の表の「`/run/beamOn` より前」を「`/run/initialize` より前」に変更)。`BeginOfRunAction` で 2 回目なら FatalException ("one /run/beamOn per macro"、TODO/01 に既にある規約)。`ApplyStepLimit()` は `Construct()` の 1 回で足りるので `BeginOfRunAction` からは外せる

**R3 ○ 受け入れテストの `_run` が落ちても `_check` は前回の出力で合格する**
- 場所: tests/acceptance/CMakeLists.txt `acceptance_run` / `acceptance_check`
- 根拠: CTest の DEPENDS は順序づけだけで、依存先の失敗でスキップされない (FIXTURES_REQUIRED と違う)。`acceptance_run` は作業ディレクトリを掃除しないので、前回の `.root` が残る。担当の実験: at1_default の出力を残したまま run を故意に失敗させても check は "all checks passed"
- 特に `AT2-1_output_name`, `AT3-1_output_name`, `AT4-1_output_name` は `test -f` だけなので、出力名の規約を壊しても前回のファイルが残る限り永久に合格する
- 修正案: `acceptance_run` を `sh -c 'rm -f ./*.root && exec "$0" "$1"'` で包む。生成テスト (AT2-4, AT3-3, AT4-3, AT-D2, AT4-4) も先頭で自分の出力を消す

**R4 ✔ `drift_scan.sh` が Stage 4 の `_raw.root` と解析マクロの `_uvw.root` `_tracks.root` を Stage 1 の入力として拾う。`range_summary.C` は Stage 2/3 の出力を二重に集計する**
- 場所: scripts/drift_scan.sh `case $name in (*[0-9]V | *_readout)`、scripts/range_summary.C `ReadFile` (`run` と `events` があれば行を出す。Stage 2/3 は `events` を CloneTree で持ち回るので条件を満たす)
- 実験: scans/am241 相当のファイル名で dry run → `_raw`, `_tracks`, `_uvw` と実データの `_tracks.root` が入力に並ぶ (計 29 本中 26 本が Stage 1 でない)。実行時は `run` が無いので失敗して `.failed` に積まれ、スクリプト全体が exit 1 になる (黙っては壊れない)。`range_summary.C` は `_2000V.root` と `_readout.root` の行を Stage 1 と同じ値で重複出力する (担当の実験)
- 修正案: パターンに `*_raw | *_uvw | *_tracks` を足し、AT-D2_dry の `touch` に 3 ファイルを足す。`range_summary.C` は `electrons` か `waveforms` があるファイルを飛ばす。AT-S3 に `range_summary.C(".", -143.9, 53.75)` の 1 ケースを足す (zMaxMm/xMaxMm は受け入れテストが一度も呼んでいない)

### 中: 条件つきで間違う、脆い、系統誤差

**R5 ✔ 入射面上の線源から −z 側に出た α は「入射点で停止」と記録され、`range_summary.C` の飛程と contained_fraction が壊れる**
- 場所: src/EventAction.cc (`fEnd` の初期値 (0,0,0))、src/SteppingAction.cc (ガス外のステップは即 return)、scripts/range_summary.C
- 実験: 全球等方 (`/gps/ang/type iso`) Ar 200 mbar 5.5 MeV 200 発 → 95 発が trackLength 0、exited 0、zEnd 0、edepTotal 0。`range_summary.C` はこれを contained (exited = 0 かつ zEnd ≤ zMax) に数え、mean_projected に 1 件あたり +250 mm (zEnd − z0 = 0 − (−250)) を足す。担当の 400 発の実験: mean_projected 169.7 mm (入った 191 発だけなら 81.9 mm)、contained_fraction 0.58 (実態は 0)
- 修正案: ガスに入らなかった α は「入射点で全エネルギーのまま出た」と記録する: `EndOfEventAction` で trackLength = 0 なら end = 一次頂点、eExit = e0、exited = 1。これで range_summary の exited_fraction と contained_fraction が正しくなる。TODO/02 に「線源は入射面上にあるので −z 向きは入らない」を明記

**R6 ○ Stage 4 は電子の無いイベントを落とす (TODO/07 の「電子が無いイベントもペデスタルとノイズだけで書く」に反する)**
- 場所: src/channel_main.cc (`summary` は `electrons` に現れたイベントだけ)、src/aget_main.cc (`summary` が空のときだけ `events` に落ちる)
- 担当の実験: 10 イベントを `-f 0.00005` で流すと summary 2 行 → raw 2 エントリ。実機の `raw` はトリガごとに 1 エントリなので、無信号イベントが消えるとイベント対応がずれる
- 修正案: `aget_main.cc` の `EventIds` の順を `events` → `summary` に入れ替える (`events` は Stage 2・3 が必ず運ぶ)。または Stage 3 の `summary` を `events` の全イベントで出す

**R7 ○ AT4-3 の 2 つの判定は固定シードで辛うじて通っている**
- 場所: tests/acceptance/check_raw.C、tests/acceptance/CMakeLists.txt (`minAreaAdc = 3000`)
- パルス面積: expected > 3000 のパルスには 5 % を素で課すが、面積の和にはノイズ σ√n が乗り、閾値付近では 5 % が 2.9 σ。担当の再計算: 227 本のうち最悪比 1.042、乱数シードを変えたときの失敗確率 4.2 %。`minAreaAdc = 5000` なら最小 5.1 σ、失敗確率 0 (76 本残る)
- FPN: 上限 pedestal + 5 σ = 480 に対し実測最大 476 (TODO/07 の「既知の弱点」)。10 イベントで失敗確率 3.5 %、100 イベントなら 30 %。さらに geometry/pads.csv は ch_graw 11/22/45/56 を 1 枚も使っていないので、`aget_main.cc` の FPN 除去分岐は受け入れテストで一度も通らず、この判定は「ノイズ発生器が 5 σ 超えを出さなかった」ことしか見ていない
- 修正案: `minAreaAdc` を 5000 に、FPN 上限を 8 σ に (実信号の最小は pedestal + 100 = 16.7 σ なので判定力は変わらない)。除去分岐を検査するなら FPN チャンネルの行を持つ小さな `waveforms` を作る専用テストを足す

**R8 ○ AT-6 の上限 1.0 に余裕が無い**
- 場所: tests/acceptance/check_energy.C `AtCheckRange(sumEdep / sumE0, 0.999, 1.0, ...)`
- 実測は比がちょうど 1 (イベント単位では 1 + 9e−16 のものがある)。加算順や最適化が変わると `value <= 1.0` が偽になりうる
- 修正案: 上限を `1.0 + 1e-9` に

**R9 ✔ 混合ガスの W と Fano は Magboltz の体積分率の線形平均で、エネルギー分配も Penning (Jesse) 効果も入っていない。α 用でなく電子用の W を使っている**
- 場所: src/Drifter.cc (`m_medium.GetW()`, `GetFanoFactor()`)、src/drift_main.cc
- 根拠: He-90-CO2-10 の `run.w = 40.47`, `fano = 0.185` は 0.9 × 41.3 + 0.1 × 33.0 と 0.9 × 0.17 + 0.1 × 0.32 に完全一致。AT-M3 の期待値もこの値なので、コードの検証にはなっているが物理の検証にはなっていない。実際の He 混合は CO2 の阻止能が大きいぶん CO2 側に多くのエネルギーが落ち、He の準安定原子が CO2 を電離する分もあるので、W はこれより小さい (文献値の推定で 10〜20 % 程度。ここでは測っていない)。純ガスでも Magboltz の W は電子用で、α 用は He 42.7 eV (電子 41.3)、CO2 34.2 eV (電子 33.0)、Ar は同じ (ICRU 31) なので、He と CO2 の一次電子数は 3〜4 % 多い
- 修正案: コードは変えず、TODO/03 と TODO/05 に系統誤差として明記する。He 混合の電子数は「下限」

**R10 ✔ CO2 の密度: Stage 1 は実在気体の NIST 値を圧力に線形にスケールし、Stage 2 (Magboltz) は理想気体**
- 場所: src/GasProperties.cc (`1.84212e-3`)、src/Drifter.cc (`SetPressure`)
- 根拠: 20 °C 1 atm の理想気体密度は 1.8295e-3 g/cm3 で、NIST 値は 0.68 % 大きい (Z = 0.993)。低圧では理想気体に近づくので、100 mbar の Stage 1 は密度を 0.68 % 過大にし、Stage 2 の E/N と 0.68 % 食い違う。He は −0.04 %、Ar は +0.08 % で無視できる。AT-2/3 の許容 3 % の中
- 修正案: 文書に残す。直すなら CO2 を理想気体値にする (AT-2 CO2 の期待値も 0.7 % 動くが許容内)

**R11 ✔ 1 mm ステップの電離電子をステップ終点に置いているので、電荷分布が飛跡に沿って約 +0.5 mm ずれ、1 mm 周期の櫛になる**
- 場所: src/SteppingAction.cc (`post->GetPosition()`)、src/drift_main.cc
- 根拠: hits の 95 % はちょうど 1 mm、CO2 100 mbar では 1 ステップ約 430 電子が 1 点に置かれる。10 cm ドリフトの σ_T ≈ 1 mm で均されるが、短いドリフトでは残る。3 mm スライスの解析には効かない
- 修正案: hits の位置をステップの中点 (または一様乱数の点) にする。TODO/01 の hits の列の説明を「ステップ終点」から変える。AT-9 の判定は変わらない

**R12 ✔ TODO/08 の較正: G = 3600 で V/W にも飽和セルがあり、V+W の総電荷は 1.3 % 目減りしている**
- 根拠 (scans/am241/am241_xm8_1600V_raw.root、先頭 300 イベント): 飽和セルは U 20,704、V 18,443、W 18,807。全セルの (ADC − 450) の和を G = 360 で走らせ直した値の 10 倍と比べると、V+W は 0.987 倍、U は 0.21 倍。TODO/08 の「V と W は G ≤ 3600 では G に比例」は 1.3 % だけ言い過ぎで、G はその分だけ (約 +1 %) 高い。他の系統誤差より小さい
- 物理担当は実データ (先頭 200 イベント) の最大 ADC が 4069 で飽和セルが無いことも報告した。シミュレーションでは V/W だけで 1 イベントあたり約 120 セルが飽和する。水平ペンシルビームの電荷集中 (線源の発散、ケージ構造、GEM の横広がりが無い) の表れで、TODO/08 の方針 (実ジオメトリまで詰めない) のまま
- 修正案: TODO/08 の結果の節に上の数字を追記する。コードは変えない

**R13 ○ `drift_scan.sh` の引数の受け渡しと失敗検知**
- `set -- $1` で「名前 電圧」を空白で割り直すので、名前に空白があると電圧がずれる (担当の実験: `Ar 200mbar_5.5MeV.root` → `-i Ar.root -v 200mbar_5.5MeV`、exit 0)
- `xargs ... || true` で xargs 自体の失敗を捨て、内側の `cd "$3" || exit 1` は `.failed` に書かない → 何も実行せずに exit 0 になりうる。macOS の xargs は `-I` の置換文字列が 255 バイトを超えると 1 件も実行しない
- `-d` (dry run) でも `mkdir -p` と `rm -f .failed` を実行する。outdir を掃除しないので条件を変えた再実行で古い出力が混ざる
- 修正案: name と v を別引数で渡す。`cd` 失敗も `.failed` に記録する。dry run の副作用を消す

**R14 ✔ 文書とコードの食い違い**
- TODO/01「検出エリアはガスボリューム全体、exited = 0 と同じ意味」と TODO/06「判定はパッド面の範囲」が矛盾。TODO/06 が正 (新しく、実測に基づく)。TODO/01 に追記する
- CLAUDE.md「pads.csv が無いと Stage 3〜4 のテストは登録されない」は不正確。AT4-1 の CLI 系 2 本は pads.csv 無しでも走る (TODO/07 の「87 テスト」が正)
- src/PhysicsList.cc のコメント: 蛍光 X 線が出ないのは production cut でなく PIXE が無効なため (option4 の既定。ログ `PIXE atomic de-excitation enabled 0`)。δ 線の説明は正しい (電子の閾値は 10 m で 172.6 keV、既定の 0.7 mm では 990 eV の下限に張り付いて 3 keV 以下の δ 線が出ていた)
- include/AgetResponse.hh のコメント「5 τ で 1.4e-3」は −8.3e-4 (符号も負)。TODO/07 の 1.166 はコードの 1.1664 に合わせる
- tests/acceptance/check_tracks.C の期待値 63.0 mm は、コメント自身の導出 (約 60.4 mm) とも実測 (60.6 mm、10 本の平均の誤差 0.6) とも合わない。60.6 に直す
- AT4-4 の `-g 1000` 固定は TODO/07 の「実装で決めたこと」に無い。結果として較正済みの既定 3600 は端から端まで一度も走っていない (単体テストの既定値確認だけ)
- TODO/03「status = StatusLeftDriftMedium または StatusLeftDriftArea」: この Garfield では −5 = StatusLeftDriftMedium、−1 = StatusLeftDriftArea で、観測は全部 −5。文書は正しいが、status では読み出し面と側壁を区別できないことを明記する

### 低: KISS、体裁、テストの質

**R15 ✔ 重複**
- `ResetGetopt` / `ToDouble` / `ToLong` が src/DriftUtils.cc, src/ChannelUtils.cc, src/AgetUtils.cc に 1 文字も違わず 3 回
- `DriftFormatNumber` は `FormatNumber` (include/GasProperties.hh) と同一。DriftUtils.cc は既に GasProperties.hh を include している
- `.root` の suffix 置換が DriftOutputName / ChannelOutputName / AgetOutputName の 3 実装 (+ RunAction.cc の `EndsWithRoot`)
- AGET の定数 (40 ns、512 セル、4 AGET、68 ch) が include/ChannelUtils.hh, src/aget_main.cc, tests/acceptance/check_raw.C の 3 か所
- scripts/scan.sh と scripts/drift_scan.sh の絶対パス化と `.failed` 集計のブロックが同一
- 修正案: ヘッダオンリーの include/CliUtils.hh に `ResetGetopt`, `ToDouble`, `ToLong`, `EndsWith` の 4 つ。`DriftFormatNumber` を消す。AGET 定数は include/AgetResponse.hh に集める。スクリプトは共通化しないほうが単純 (差分が小さい)

**R16 ✔ 未使用**
- `Drifter::TableWasGenerated()` と `GasFile()` (include/Drifter.hh): 呼び出し 0。AT2-4 はログの文字列で判定している
- tests/acceptance/check_drift.C の `status`: 読むだけで使わない
- `Pad::padNo`, `Pad::chGeom`: アプリは使わない (pads.csv の全列を持つ一貫性のため。残してよい)

**R17 ○ 命名の流儀が 3 種類**: Stage 1 は Geant4 流の `fConfig`、Drifter は `m_medium`、PadMap は `pads_`。Geant4 非依存の 2 つは片方に揃える (触る機会に)

**R18 ✔ 構造**
- src/aget_main.cc は `waveforms` の全行を `std::map<int, vector>` に貯めてから書く。行はイベント順なので channel_main.cc と同じ「1 イベントずつ流して flush」のほうが単純でメモリも一定
- channel_main.cc / aget_main.cc は `electrons` / `waveforms` がイベント順に並んでいることを暗黙に仮定 (hadd などで崩れると summary が分裂)。コメントで明記してある。現状の Stage 2/3 は順序を保つ
- src/channel_main.cc の `run` の読み (9 列) と書き (13 列) が同じ列名を 2 度書く。ファイル内の 2 関数に分けると読みやすい

**R19 ○ 常に真、または余裕の読み方が難しい判定**
- tests/acceptance/check_gps_energy.C の min ≥ 1、max ≤ 6 は Lin 分布の定義域なので破れない (効くのは σ ≥ 1 だけ)
- check_readout.C の `perChannel.size() <= 256` は直前の `badChannel == 0` から自明。cz 下限 −250 は到達不能 (板の最小 cz は −250 + 0.433)
- check_raw.C の「ペデスタル平均が 3 σ 以内」は 20 セルの平均に単セルの 3 σ を課している (実効 13 σ)。仕様どおりだが表示が誤解を招く
- AT-M2 Ar-90-CO2-10 は許容 ±5 % の 62 % を系統差 (3.1 %、飛程加法則と Geant4 の元素ベース阻止能の差) が使っている。統計誤差は 0.04 %
- AT-10 (hits on/off) の比 1.0024 は統計的に有意 (4.5 σ) な実在の差 (1 mm のステップ制限がエネルギー損失の積分を変える)。許容 1 % の中だが「同じ物理」ではないことを文書に残す
- `AT2-1_no_hits` だけが共有 gascache を引数に取りながら AT2-4_cache に DEPENDS していない。今は hits チェックが Drifter 構築より先なので安全

**R20 ○ 受け入れテストが通っていない経路** (担当が手で動かして正しく動くことは確認済み): 3 つの CLI の `-n`、aget-shaper の `summary` 欠落時と両方欠落時のフォールバック、`range_summary.C` の zMaxMm/xMaxMm、`drift_scan.sh` の入力 0 本、R1/R2 の異常系

**R21 ✔ 公開リポジトリに実機ジオメトリ由来の数値がある (発注者の判断: 入れたままでよい、2026-09-07)**
- tests/unit/test_readout_utils.cc の実 pads.csv のテストは、板の外形 (発注者が会話で示した値) に加えて、先頭パッドの重心・頂点座標とその AGET/チャンネル割当を数値で持つ。TODO/06 には座標変換の基準値 (パッド上流端の y_det) と pads.csv の md5、scripts には板の範囲 (−143.9, 53.75) がある
- 発注者の判断で現状維持 (pads.csv 本体と channel_map.csv を入れない方針は変わらない)

## 却下した指摘 (裏取りで否定)

- 物理担当「G = 3600 は飽和で約 1.4 倍過大、真の G は 2500 前後」: 総電荷 (全ビュー) の 29 % 損失は U の飽和で、較正は最初から V+W で行っている (TODO/08)。V+W の損失は 1.3 % (R12)。結論は却下、数字は R12 に採用
- 物理担当「δ 線はカットが 1 µm でも出ない (10 m は効いていない)」: 電子の閾値は 990 eV が下限で T_max ≈ 3 keV より低いので、既定のカットでは δ 線が出る。実際に 10 m にする前は hits の 85 % が δ 線だった (TODO/01)。蛍光の説明 (PIXE 無効) だけ採用 (R14)
- 物理担当「TODO/08 の高さ 59.5 mm と実行条件 (y = 0) が食い違う」: TODO/08 は「y = 0 に置いて窓を 7.26 µs 遅らせるのと等価」と明記している。違うのは拡散幅だけ (σ_T 0.99 mm 対 0.77 mm)。却下
- 物理担当「pads.csv の座標変換は鏡映なので V/W が入れ替わっている可能性」: x_sim = x_det、z_sim = −y_det は、読み出し面を +y から見下ろした右手系そのもので 3 次元では鏡映でない。図面を基板のどちら側から見たかは発注者が確認済み (TODO/06)。現状の較正 (U に平行な飛跡、V = W) では検証できないことだけ記録する

## 問題なしと確認した点

- Garfield の乱数: `Random::SetEngine(T engine)` は値渡しで `std::bind` がコピーを持つ。同じ入力で 2 回走らせて 719 電子が全行一致。ガステーブルの生成経路と読み込み経路で W/Fano が同じ
- AvalancheMC の境界処理: 読み出し面到達電子の y + 100 は RMS 8e−5 mm (二分探索)。±0.5 mm の判定は取りこぼしも取り込みもしない
- 電場の向き (+y、電子は −y へ)、E = V/20 cm、mm↔cm、cm/ns→cm/µs、µs→ns の換算、GPS の向きの規約 (dz0 = +1 が +z)
- Magboltz CO2 100 mbar 60 V/cm 0.4341 cm/µs と analyzeUVW.C の 0.7233 × 0.6 = 0.4340 が 0.03 % で一致
- チャンネル地図: (aget, ch_graw) と (strip_dir, strip_no) が 256 対 256 の全単射。パッドは隙間なく敷き詰められている (到達電子の 99.81 % がパッド上)。頂点は sim 座標で反時計回り
- AGET 整形: 223 ns でピークがちょうど 223.000 ns、`kAgetPeakX = 1.1664` は 3 sin x (1 − x) + x cos x = 0 の根。5 τ の打ち切り損失は無視できる
- 核阻止能は有効 (`nuclearStopping: for alpha`)。エネルギー保存 (AT-6)。ステップごとの Fano 揺らぎは加法的で正しい
- `run.gas` は Char_t リーフなので `char[64]` は正しい。全 check マクロの SetBranchAddress の型が実出力と一致
- 単体テスト 93 ケースは `--gtest_shuffle` 3 シードで全部合格 (getopt のリセットは順序に依存しない)。pads_synthetic.csv は辺 1.000 mm の菱形 (数値計算で確認)
- ガステーブルの書き出しは tmp + rename で原子的。共有 gascache を使うテストは AT2-1_no_hits 以外すべて AT2-4_cache に DEPENDS 済み。作業ディレクトリの衝突なし
- 許容幅の余裕: AT-2/3 は 0.02〜0.14 % (許容 3 %)、AT-4 最大 1.4 % (5 %)、AT2-3 の電子数 0.9987〜1.0008 (5 %)、AT3-3 の総和一致 1e−6、AT4-3 のプールしたペデスタル 450.03、σ 6.000。AT-7 は 1000/1000 イベントが異なるシードで相違
- scan.sh は空白入りの outdir と相対 `-x` で正しく動く。`.failed` の並列追記は 200 件で破損なし。`set -eu` 下の dry run の分岐は sh と dash で意図どおり
- エラー処理の流儀は層ごとに一貫 (純関数は例外、CLI は fprintf + return 1、マクロは G4Exception)。コードに非 ASCII なし。TODO/FIXME なし。ROOT 出力の全ツリーの列名・型・順序が TODO/01, 03, 06, 07 の表と一致。CLI の既定値が TODO/07 と CLAUDE.md に一致

## 次回の作業案 (優先順)

各項目は TODO/10 として発注書 (受け入れテスト付き) にしてから Opus/Sonnet に出す。

1. R1, R2: `/tpc/` の 3 コマンドを PreInit 限定、2 回目の beamOn を拒否、受け入れテスト 2 本
2. R3, R7, R8, R19 末尾: `acceptance_run` の掃除、`minAreaAdc` 5000、FPN 8 σ、AT-6 の上限、`AT2-1_no_hits` の DEPENDS
3. R4, R13: `drift_scan.sh` の除外パターンと引数、`cd` 失敗の記録、dry run の副作用、`range_summary.C` の Stage 2/3 スキップ、AT-D2_dry と AT-S3 の追加
4. R5, R6: 入らなかった α の記録、Stage 4 のイベント一覧
5. R15, R16, R18: CliUtils.hh、`DriftFormatNumber` と未使用メンバの削除、aget_main の逐次化
6. R9〜R12, R14: 文書 (TODO/01, 03, 05, 08, CLAUDE.md、コメント)、check_tracks.C の期待値
7. R21: 現状維持と決定済み。作業なし
