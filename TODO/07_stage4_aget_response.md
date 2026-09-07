# 07 Stage 4: AGET 応答と実データ形式への変換 (aget-shaper)

前提: TODO/01〜06。状態: 実装済み (2026-09-07)。AT4-1〜AT4-4 合格、全 112 テスト合格。Stage 3 の `waveforms` を入力に、実機の生データと同じ ROOT 形式を作る。

検証結果 (発注者による):

- AT4-2: カーネル 24 サンプル、デルタ入力のピークは +6 サンプル、振幅は理想値の 0.987 倍 (40 ns の離散化)、アンダーシュート −1 %
- AT4-3 (He 200 mbar 0.3 MeV, 2000 V, G = 1000): 最大 ADC 1228 (ペデスタル上 778)、飽和なし。パルス面積は電子数 × G × e × 4096/レンジ × カーネル面積と ±5 % で一致
- AT4-4 (CO2 100 mbar, 2 MeV, 30° 傾き, 1200 V, `-w 20`): 実データ用の analyzeUVW.C と makeTracks.C がそのまま走り、10 イベント中 10 で ok = 1、ドリフト方向の方向余弦 0.505 (期待 0.5)、length の平均 60.7 mm。最大 ADC はペデスタル上 3070 で、実データの約 3500 と同程度 (G = 1000 は出発点として妥当)
- makeTracks.C の length は 3 mm スライス重心の 2 %/98 % 分位点で端点を決めるため、真の飛程 (Geant4 経路長 73.2 mm、直線広がり 70.9 mm) より系統的に短い (真のエネルギー付与に同じ定義を当てても 66.4 mm、端のスライス重心の内側寄りで更に約 6 mm 減)。整形とノイズを切っても 60.3 mm なので Stage 4 の問題ではない。飛程を測る解析では端点の外挿などが要る
- pads.csv が無い環境: configure は警告 2 つで成功、87 テスト (Stage 1〜2 と Stage 4 の CLI) が合格
- make_channel_map.py: 実物の channel_map.csv と 256 行すべて一致 (s_mm の最大差 8×10^−4 mm、pads.csv の丸めによる)

実装で決めたこと (発注書からの変更):

- AT4-4 の length の期待値は 63.0 mm ±10 % (上の理由)。AT4-3 のペデスタル σ の判定は「外れ 1 % 以下 + 全チャンネルをプールした σ が ±5 %」に変更 (20 セルの RMS 推定量のばらつきのため)。パルス面積の判定にノイズ 3σ√n の許容を追加
- 出力ファイルには `raw` ツリーだけ (graw2root.C と同一)。イベント一覧は Stage 3 の `summary`、無ければ `events`
- AgetResponse.hh はヘッダオンリー (受け入れマクロが同じ式で再計算する)
- 既知の弱点: AT4-3 の FPN 判定 (最大 476 < 480) の余裕が 4 カウントしかない。固定シードなので再現するが、イベント数を増やすと破れる。d ≠ 0 の pads.csv では make_channel_map.py の V/W の s_mm が 0.866 d ずれる (コメントに明記)
- AT4-4 と AT4-3 は較正値 (G = 3600) と独立にシェーパー自体を検査するため `-g 1000` を固定している。較正済みの既定 3600 は端から端までのテストでは走っていない (単体テストの既定値確認のみ)。AT4-3 のパルス面積判定は `minAreaAdc = 5000` 以上のパルスにだけ 5 % の許容を課す 2 段判定 (閾値付近はノイズ σ√n の寄与が相対的に大きいため)

## 実機の設定 (2026-09-07 に記録。解析と次のシミュレーションの前提)

### 高電圧 (CAEN DT1415ET、GEM-HV)

大型 TPC の画面の値だが、mini eTPC もまずほぼ同じ電圧で運転している。アノードストリップ側から順にチャンネルが直列に積まれ、合計がカソード電位になる。

| CH | 区間 | 設定 [V] | 電流 [µA] |
|---|---|---|---|
| 0 | TR1 (GEM1 とアノードストリップの間、誘導ギャップ) | 360 | 0.024 |
| 1 | GEM1 | 240 | 0.064 |
| 2 | TR2 (GEM2 と GEM1 の間) | 300 | 0.059 |
| 3 | GEM2 | 240 | 0.088 |
| 4 | TR3 (GEM3 と GEM2 の間) | 300 | 0.028 |
| 5 | GEM3 | 240 | −0.021 |
| 6 | DRIFT1 | 664 | 6.812 |
| 7 | DRIFT2 | 664 | 6.821 |
| 合計 | カソード | 3008 | |

- 増幅は 3 枚の GEM (GEM1 が最もアノード側)。各 GEM 240 V は 100 mbar の CO2 での値
- ドリフト部は 2 チャンネル分 (1328 V)。実際のランのドリフト電圧は run 2026-09-01 が 1200 V、run 2026-09-02 が 1600 V (現行条件も 1600 V)。tpcdaq-rs の analyzeUVW.C の定数 DRIFT_HV_V = 1200 は 09-01 用で、09-02 に使うと z スケールが 4/3 ずれる。ランごとに変わるので、シミュレーションではドリフト電圧 (`-v`) と GEM の実効ゲインを別々の引数にする
- GEM 電圧を変えてゲインの変化を見るのは将来の楽しみ。ゲインの電圧依存はデータの実測曲線か Garfield++ の微視的計算が要るので、まずは実効ゲインを引数で与える

### AGET (GET エレクトロニクス)

- ピーキング時間 223 ns、ゲインレンジ 120 fC (フルスケール)、サンプリング 25 MHz (40 ns/セル)、512 セル、12 bit ADC
- これも引数で変えられるようにする (ピーキング時間は 16 段階、レンジは 120/240/1000/10000 fC)
- 実データ (run 2026-09-02) から: パルスは正極性、ペデスタルはチャンネルごとに約 400〜490 カウント、ノイズ σ は約 5〜9 カウント、大きな信号はペデスタル上 3500 カウントに達し 4095 で飽和し得る
- FPN チャンネル 11, 22, 45, 56 は信号なし (解析マクロは channel_map で除外する)

### 実データと解析マクロ

- 場所: `/Users/aogaki/Workspace/tpcdaq-rs/macros` (README.md、graw2root.C、analyzeUVW.C、makeTracks.C、analyzeTracks.C、channel_map.csv、make_pads.py)。データは `macros/data` に run 2026-09-01 と 2026-09-02 の GRAW、`_raw.root`、`_uvw.root`、`_tracks.root`
- 解析の流れ: graw2root.C (GRAW → `raw`) → analyzeUVW.C (`raw` → `uvw`: ペデスタル引き算、閾値、チャンネル地図、時間セル → ドリフト z、ビューごとの直線、3 mm スライスの重心) → makeTracks.C (`uvw` → `tracks`: 3D 直線飛跡、長さ、電荷)
- analyzeUVW.C の定数: DRIFT_HV_V 1200、DRIFT_LEN_CM 20、PRESSURE_MBAR 100、SAMPLING_MHZ 25、ADC_THRESHOLD 35、ペデスタル窓はセル 1〜20、SEED_ADC 100、BAND_MM 10、SLICE_MM 3。ドリフト速度は v = 0.7233 × E/p (CO2 専用、TODO/06 で Magboltz と一致を確認)
- makeTracks.C は 3 mm スライスが 3 つ以上ないと ok = 0 にする。ドリフト方向に広がりのない飛跡 (α 線源軸に平行で高さ一定) は飛跡にならないので、閉ループ試験では α に傾きを付ける

## 出力形式 (決定)

graw2root.C が作る `raw` ツリーと同じにする。これで analyzeUVW.C と makeTracks.C がそのまま走る。

| ブランチ | 型 | 意味 |
|---|---|---|
| eventId | UInt_t | Stage 1 の eventID |
| eventTime | ULong64_t | 48 bit の CoBo タイムスタンプ相当。シミュレーションでは eventID × 10^8 (単調増加なら何でもよい) |
| adc[4][68][512] | Short_t | AGET、生チャンネル、時間セルごとの ADC 値 (0〜4095) |

- 1 エントリ = 1 イベント。電子が無いイベントもペデスタルとノイズだけで書く
- ファイル名は入力の `_readout.root` を `_raw.root` に置き換える (analyzeUVW.C が `_raw.root` を `_uvw.root` に置き換えるため)

## モデル

1. 入力電荷: チャンネル c、ビン k の一次電子数 N[c][k] (Stage 3 の electrons) に実効ゲイン G を掛け、電荷 q[c][k] = N G e [fC] (e = 1.602×10^−4 fC)。G はまず引数 (`-g`)。GEM のゲイン揺らぎは入れない
2. 整形: GET の解析で使われる経験式 h(t) = (t/τ)^3 exp(−3 t/τ) sin(t/τ) (t ≥ 0) をピークで 1 に正規化する。ピークは t = 1.1664 τ なので、ピーキング時間 t_p に対し τ = t_p / 1.1664。t_p = 223 ns なら τ = 191 ns、半値幅 242 ns、零交差 601 ns、アンダーシュートはピークの −1 %。実機のパルサー波形で式と τ の対応を確認すること
3. 離散畳み込み: v[c][k] = Σ_j q[c][j] h((k − j) × 40 ns)、k − j ≥ 0。h は 40 ns 刻みでサンプルする (τ の 5 倍で打ち切ってよい)
4. ADC: adc = pedestal + round(v × 4096 / range) + noise。range は fC のフルスケール (`-r`、既定 120)、pedestal は `-p` (既定 450)、noise は σ (`-s`、既定 6) のガウス乱数。0〜4095 に丸める。FPN チャンネルはペデスタルとノイズだけ
5. 時間の原点は Stage 3 の窓 (`-w`) のまま。整形による遅れ (約 1 ピーキング時間) はそのまま出る

将来の拡張 (今は入れない): GEM ゲインの電圧依存と揺らぎ、GEM 通過の時間遅れと横拡散、チャンネルごとのペデスタル表 (実データの `raw` から取れる)、クロストーク、AGET の入力容量によるノイズ差。

## CLI

```
aget-shaper -i <stage3_readout.root> [-t <peaking ns>] [-r <range fC>] [-g <gain>] [-p <pedestal>] [-s <noise sigma>] [-n <maxEvents>] [-h]
```

- `-i` は必須。既定: `-t 223`、`-r 120`、`-g 3600` (run 2026-09-02 の 241Am データで較正。TODO/08 の「結果」節)、`-p 450`、`-s 6`
- 入力に `waveforms` が無ければ終了コード 1
- 乱数は固定シード (再現性)

## 受け入れテスト

### AT4-1 CLI と名前

- `-i` なし → 終了コード 1。`waveforms` の無い入力 (Stage 2 の出力) → 終了コード 1
- `X_2000V_readout.root` → `X_2000V_raw.root`。`raw` ツリーに eventId, eventTime, adc[4][68][512] がある

### AT4-2 デルタ応答 (単体テスト)

- 1 つのビン k0 に N 電子を置いた入力 → ピークは k0 + 6 サンプル (223 ns / 40 ns = 5.6)、振幅は N G e × 4096 / range の ±2 % (サンプリングの離散化分)、ピーク後 20 サンプル (800 ns) 以内に |v| がピークの 2 % 以下、アンダーシュートは負でピークの 2 % 以内
- ピーキング時間を 2 倍にするとピーク位置が約 2 倍遅れ、振幅は同じ

### AT4-3 端から端まで (He 200 mbar 0.3 MeV, 2000 V, `-f 0.1`)

- `raw` に 10 エントリ。全チャンネルでセル 1〜20 の平均がペデスタルの ±3 σ 以内、標準偏差が σ の 0.5〜1.5 倍
- FPN チャンネル (11, 22, 45, 56) の最大値がペデスタル + 5 σ 未満
- 1 チャンネル以上でペデスタル上 100 カウントを超える正のパルスがある。全値が 0〜4095
- 各チャンネルのパルス面積 (ペデスタル引き算後の和) が Stage 3 の electrons の和 × G e × 4096/range × h の面積 (40 ns 単位) の ±5 %

### AT4-4 実データの解析マクロによる閉ループ

- Stage 1: CO2 100 mbar、2 MeV、hits true、`/gps/direction 0 0.5 0.866` (ドリフト方向に 30° 傾ける)、10 イベント。CSDA 飛程 72.8 mm なのでパッド面に収まり、ドリフト方向に 36 mm 広がる
- Stage 2: `-v 1200 -f 0.1` (analyzeUVW.C の定数と同じ条件。v_drift = 0.434 cm/µs)。Stage 3: `-w 20` (ドリフト 10〜13.6 cm は 23〜31 µs で、窓は 20 µs から)。Stage 4: 既定値
- analyzeUVW.C と makeTracks.C を走らせ、`tracks` の 10 イベント中 8 以上が ok = 1、その length の平均が 72.8 mm の ±15 % 以内、ドリフト方向の方向余弦の絶対値の平均が 0.5 の ±0.1
- 解析マクロは tpcdaq-rs から `external/tpcdaq-macros/` にコピー済み (出典と md5 は NOTICE.md)。channel_map.csv と geometry/pads.csv は実機ジオメトリ由来なので git に入れない (gitignore 済み)。テストは `scripts/make_channel_map.py` で pads.csv から作った channel_map.csv を作業ディレクトリに置いて走らせる

## ジオメトリ由来ファイルの扱い (2026-09-07 決定)

- `geometry/pads.csv` と `channel_map.csv` は実機のジオメトリ (.dat) から作る派生物で、tpcdaq-rs でも gitignore されている。このリポジトリでも git に入れない。手元のファイルはそのまま使う
- 生成手順: `external/tpcdaq-macros/make_pads.py <geometry.dat> geometry/pads.csv [d_mm] [channel_map.csv]` で pads.csv、`scripts/make_channel_map.py geometry/pads.csv <out.csv>` で channel_map.csv
- channel_map.csv の列は `aget,raw_channel,view(0=U 1=V 2=W),strip,s_mm`。s_mm は検出器座標でのストリップ位置 = ストリップ上の任意のパッド重心 (x_det, y_det) とピッチ方向単位ベクトル p の内積。x_det = x_sim、y_det = 48.930 − (z_sim + 250 − d)。ストリップ方向は U 90°、V −30°、W +30° で、p_U = unit(−(u_W + u_V))、p_V = unit(u_U + u_W)、p_W = unit(u_V − u_U) (make_pads.py と同じ定義)。手元の実物 channel_map.csv と 256 行すべて 10^−3 mm 以内で一致することを確認する (実物があるときだけ走る単体テスト)
- pads.csv が無い環境でもビルドと Stage 1〜2 のテストは通ること。Stage 3〜4 の受け入れテストと 13,010 パッドの単体テストは CMake で `if(EXISTS geometry/pads.csv)` のときだけ登録し、無ければ警告を出す。PadMap の基本の単体テストは tests/unit に置いた小さな合成 CSV (数枚の菱形) で行う

## 実装タスク (TDD の順)

0. 上記「ジオメトリ由来ファイルの扱い」: `scripts/make_channel_map.py`、pads.csv が無いときのテストの条件登録、合成 CSV による PadMap 単体テスト
1. 整形関数と離散畳み込み (純関数)。AT4-2
2. CLI、出力名、ADC 変換、`raw` ツリーの書き出し。AT4-1、AT4-3
3. tpcdaq-rs のマクロを取り込み、AT4-4
4. (未) 実効ゲイン G の較正: 実データの α 飛跡の総電荷 (analyzeUVW の hCharge) と、同じ条件のシミュレーションの総電荷を比べて G を決める。241Am の線源位置と向きの情報が要る
5. TODO/01 に Stage 4 を追記、CLAUDE.md の規約に `aget-shaper` を追記

## 相談したいこと

- (済) α 線源は 241Am (主線 5.486 MeV、5.443 MeV が 13 %)。線源の位置と向きは未確認 (実データの飛跡は検出器を突き抜ける)
- (済) tpcdaq-rs のマクロのコピーは OK。channel_map.csv と pads.csv は git に入れない
- (済) パルサー波形は無い。macros/data の 2 ラン (2026-09-01: 20,724 イベント、1 イベントに 100〜150 チャンネル、2026-09-02: 12,130 イベント) が 241Am の α のデータで、較正はこれに対して行う。パルスは α の電荷到達の広がり (µs 幅) と整形の畳み込みなので、整形の式は経験式のまま使う
