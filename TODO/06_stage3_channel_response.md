# 06 Stage 3: パッド・ストリップ・チャンネルへの振り分けと時間ビン化

前提: TODO/01〜05。状態: 実装済み (2026-09-07)。AT3-1〜AT3-4 合格、全 95 テスト合格。

検証結果 (発注者による):

- AT3-2: 13,010 パッドすべてで重心が自分のパッドに落ちる
- AT3-3 (He 200 mbar 0.3 MeV, 2000 V, `-f 0.1`, 10 イベント): onReadout/total = 0.941、onPads/onReadout = 0.998、inWindow/onPads = 1.000。電子は 127 チャンネルに載り、上位 5 チャンネルはすべて U ストリップ (x = 0 の α 線源軸に沿った飛跡は U ストリップ 1〜2 本に集中する)。cz は −249.6 から −159.5 mm、bin は 326 から 401 (13.0 から 16.1 µs)
- range_summary.C の `contained_fraction` をパッド面 (`-143.9, 53.75`) で評価すると、He 200 mbar の 0.3 MeV は 1.0、1.0 MeV は 0 (飛程 158 mm > 106.5 mm)

実装で決めたこと (発注書からの変更):

- `PadMap` はヘッダオンリー (include/PadMap.hh)。受け入れテストの ROOT マクロが同じ探索コードを include して独立に再計算するため
- AT3-3 の cz 上限は −155 mm。10 イベントの停止点は −183 から −167 mm に散らばるので、平均停止点でなく最遠の停止点 + 3σ を使った
- `summary` は `electrons` に現れたイベントだけ 1 行
- `-w` は負の値も可。`d` の確定後は pads.csv を再生成するだけでコードの変更は不要

## 読み出し板の実態 (2026-09-07 に確定)

- パッドは辺 1.0 mm の菱形 (内角 60°/120°)、13,010 枚。向きは 3 種類で、U/V/W いずれかのストリップに電気的に束ねられている
- ストリップは 256 本 = 256 チャンネル (AGET 0〜3 × 64 ch)。エレクトロニクスが読むのはストリップ
- パッド面の実寸: x ∈ [−53.75, +53.75] (107.5 mm)、α 線源軸方向 106.5 mm。ドリフト長 200 mm。有感体積は約 10.8 × 10.7 × 20 cm
- 20 × 20 × 50 cm のガスボリュームは容器かフィールドケージ外形の値と思われ、ハードウェア側で確認待ち。シミュレーションの幾何はそのまま、「検出エリア内で止まる」の判定はパッド面の範囲で行う
- サンプリング 25 MHz (40 ns/セル)、1 イベント 512 セル = 20.48 µs。ドリフト 200 mm のうち窓に入るのは 100 mbar CO2 (0.579 cm/µs) で 119 mm 分。どの帯が入るかはトリガ遅延で決まる (.dat の公称値 10.24 µs)
- 座標変換 (pads.csv のヘッダに記載): x_sim = x_det、z_sim = −250 + d + (48.930 − y_det)、d = 入射窓からパッド上流端までの距離 [mm]。暫定 d = 0
- 注意: シミュレーションの z 軸は α 線源軸 (基板図の −y_det)。加速器ビーム軸は基板図の +x_det で、sim の +x に対応する。ドキュメントとコードのコメントでは「ビーム軸」でなく「α 線源軸」と呼ぶ
- 現行の運転条件: CO2 100 %、100 mbar、293 K、80 V/cm (1600 V)。ドリフト速度 0.579 cm/µs (運転点表の係数由来)。Magboltz との比較値はこの文書の末尾

## geometry/pads.csv

`/Users/aogaki/Workspace/tpcdaq-rs/macros/pads.csv` (md5 3f51bf63) をそのまま `geometry/pads.csv` に置いた。列:

```
pad_id,strip_dir,strip_no,pad_no,aget,ch_graw,ch_geom,cx,cz,x0,z0,x1,z1,x2,z2,x3,z3
```

- 単位 mm、シミュレーション座標系 (x, z)、頂点は反時計回り。`#` で始まる行はコメント
- ch_graw (0〜67) が実データと同じ生チャンネル番号。ch_geom (0〜63) は FPN を除いた番号
- 検算済み: 辺長 1.000 mm、256 ストリップ = 256 (aget, ch_graw)

## ゴール

```
channel-response -i He_200mbar_0.3MeV_2000V.root -p geometry/pads.csv [-w <window start, us>]
```

で `He_200mbar_0.3MeV_2000V_readout.root` ができ、イベントごと・チャンネルごとの 40 ns ビンの電子数が得られる。

## 設計判断

- 入力は Stage 2 の出力 (`electrons`, `events`, `run`)。読み出し面 y = −100 mm (±0.5 mm) に到達した電子だけを使う
- パッドの探索: パッド面を 1 mm 格子に区切り、各格子にバウンディングボックスが重なるパッドを登録しておく。電子の (x, z) の格子からたかだか数枚の候補を取り、凸多角形の内外判定 (反時計回りの外積がすべて 0 以上) で決める。10^6 電子でも数秒
- 時間ビン: bin = floor((t − w0) / 40 ns)、0〜511 だけ採用。w0 は `-w` [µs]、既定 0 (= α 生成時刻)。トリガ遅延の流儀は実験側のものなので、シミュレーションでは「窓の開始時刻」だけを持つ
- AGET の整形応答 (ピーキング時間) の畳み込みは後段の別ステップにし、ここでは入れない
- 出力は疎な形式。電子が 1 個以上あった (event, channel, bin) だけ行にする

## CLI

```
channel-response -i <stage2.root> -p <pads.csv> [-w <us>] [-n <maxEvents>] [-h]
```

- `-i`, `-p` は必須。欠けていれば usage を stderr に出して終了コード 1
- 入力に `electrons` が無ければ終了コード 1
- 出力名は入力名の `.root` の前に `_readout` を付ける

## ROOT 出力の構造

ntuple `waveforms` (1 行 = 電子が 1 個以上ある (event, channel, bin))

| 列 | 型 | 意味 |
|---|---|---|
| eventID | I | Stage 1 のイベント番号 |
| aget | I | AGET 番号 0〜3 |
| ch_graw | I | 生チャンネル 0〜67 |
| strip_dir | C | U, V, W |
| strip_no | I | ストリップ番号 |
| bin | I | 時間ビン 0〜511 |
| electrons | D | 重みの和 |

ntuple `summary` (1 行 = 1 イベント)

| 列 | 型 | 意味 |
|---|---|---|
| eventID | I | |
| total | D | Stage 2 の電子の重みの和 (全部) |
| onReadout | D | 読み出し面に到達した分 |
| onPads | D | さらにパッドに載った分 |
| inWindow | D | さらに時間窓に入った分 |

ntuple `run`: Stage 2 の `run` の列 + padsFile (S), windowStartUs (D), binNs (D) = 40, nBins (I) = 512
Stage 2 の `events` を CloneTree でコピーする。

## 受け入れテスト

### AT3-1 CLI

- `-i` または `-p` が無い → 終了コード 1。`electrons` の無い入力 (Stage 1 の出力) → 終了コード 1
- 出力名: `X_2000V.root` → `X_2000V_readout.root`

### AT3-2 パッド探索 (単体テスト)

- geometry/pads.csv を読み、13,010 枚すべてで重心 (cx, cz) がそのパッド自身に落ちる
- パッド面の外の点 (x = 0, z = 0) と (x = 60, z = −200) は「パッドなし」
- 頂点 x1 を 0.01 mm だけ内側に寄せた点はそのパッド

### AT3-3 端から端まで

Stage 1 (He 200 mbar 0.3 MeV, hits true, 10 イベント) → Stage 2 (`-v 2000 -f 0.1`) → Stage 3 (`-w 0`)。

- `waveforms` の electrons の総和が `summary.inWindow` の総和と一致 (1e-6)
- 全行で bin が 0〜511、(aget, ch_graw) が pads.csv に存在する
- `summary`: onReadout / total が 0.9 以上 (入射面から抜けた分だけ減る)、onPads / onReadout が 0.99 以上 (α 飛程 74 mm はパッド面 106.5 mm に収まる)、inWindow / onPads が 0.99 以上 (ドリフト 10 cm は 14.4 µs で窓 20.48 µs に入る)
- 電子が載ったパッドの cz の範囲が α の飛跡 (z ∈ [−250, −176 ± 拡散 4 mm × 3]) に収まる

### AT3-4 時間窓

- 同じ入力を `-w 30` (30 µs、到達時刻の最大 15 µs より後) で走らせると `waveforms` は 0 行、`summary.inWindow` は全イベントで 0

## range_summary.C への追加

- 引数 `zMaxMm` (既定 250) と `xMaxMm` (既定 100) を足し、列 `contained_fraction` (exited = 0 かつ zEnd ≤ zMaxMm かつ |xEnd| ≤ xMaxMm の割合) を追加する。パッド面なら `zMaxMm = -143.9, xMaxMm = 53.75`
- 受け入れテスト: AT-S3 の判定に contained_fraction 列の存在と値域 [0, 1] を足す

## 単体テスト (GoogleTest)

- CLI 解析、出力名、時間ビン計算 (負の t − w0 は捨てる、511 まで)
- pads.csv の読み込み (行数 13,010、列の順、コメント行の無視)
- 凸多角形の内外判定と格子探索 (AT3-2)

## 実装タスク (TDD の順)

1. pads.csv の読み込みと格子探索 (純関数、Geant4/Garfield/ROOT 非依存)。AT3-2
2. CLI、出力名、時間ビン
3. 本体: 入力読み込み → 振り分け → `waveforms`, `summary`, `run`, `events`
4. AT3-1, AT3-3, AT3-4
5. range_summary.C の contained_fraction
6. (済) TODO/01 の座標系の説明に α 線源軸の注記を追加
7. 小さな修正 2 つ: `ParseGasSpec` が成分名の重複 (`He-50-Ar-40-He-10`) を `std::invalid_argument` で弾く (単体テスト追加)。`scripts/drift_scan.sh` が `*_readout.root` を入力から除外する (AT-D2 の dry run に readout ファイルを置いて確認)

## Magboltz 検算 (CO2 100 %, 100 mbar, 293.15 K)

Garfield++ 2025.12 の Magboltz、ncoll = 10 (統計誤差 0.1 %)。

| E [V/cm] | v_drift [cm/µs] | D_L [√cm] | D_T [√cm] |
|---|---|---|---|
| 60 | 0.433 | 0.032 | 0.032 |
| 80 | 0.577 | 0.028 | 0.029 |
| 100 | 0.723 | 0.028 | 0.027 |

運転点表由来の 0.579 cm/µs (80 V/cm) と 0.3 % で一致する。運転点表の係数 0.7233 cm/µs per (V/cm/mbar) は Magboltz の 100 V/cm・100 mbar の値 0.7234 と一致し、この領域では v_drift が E/p に比例することも合う。10 cm ドリフト後の横広がりは σ = 0.9 mm。
