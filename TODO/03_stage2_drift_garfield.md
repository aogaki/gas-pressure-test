# 03 Stage 2: 電離電子のドリフト (Garfield++)

前提: TODO/01_architecture.md, TODO/02_stage1_alpha_range.md。Stage 1 を `/tpc/hits true` で走らせた ROOT ファイルを入力にする。
状態: 実装済み (2026-09-07)。AT2-1〜AT2-4 合格、全 69 テスト合格。

検証結果 (発注者による):

- AT2-2 (Ar 200 mbar, 100 V/cm, 電子 1000 個): 到達時刻の平均が期待値の 0.998 倍、x の標準偏差が D_T √L の 1.006 倍
- He 200 mbar 0.3 MeV 100 イベント、2000 V、`-f 0.1`: 電子数 7262/イベント (e0/W = 7264)、到達点の広がり σ_x = 3.97 mm (D_T √L = 3.99 mm)、ドリフト時間 14.4 µs (期待 14.4 µs)、読み出し面到達率 94 %
- 実行時間: Ar 200 mbar 5.5 MeV 10 イベントで `-f 1` が 37 s (2.1×10^6 電子、56 MB)、`-f 0.01` が 0.6 s。Magboltz のテーブル生成は初回のみ 76 s

実装で決めたこと (発注書からの変更):

- α は入射面 z = -250 mm から始まるので、最初のヒットの電子の一部が拡散で入射面から抜ける (Ar 5.5 MeV で 2 %、He 0.3 MeV で 6 %)。AT2-3 の判定は「ガス中で止まった電子が 0」「読み出し面到達率 90 % 以上」「到達時刻は読み出し面に着いた電子だけで比較」に変えた。実機で窓から入射する場合も窓際の電子は失われるので、物理的にも妥当
- 読み出し面に着いた電子も入射面から抜けた電子も AvalancheMC の status は同じ -5 (StatusLeftDriftMedium)。区別は終点座標 (y = -100 mm か) で行う
- `-f` の間引きは確率的丸め floor(n f + U) で不偏にした
- Garfield++ の乱数エンジンを固定シード (RandomEngineSTL, seed 1) にして再現性を確保。電子数サンプリングは std::mt19937 (seed 12345)
- 1 点のガステーブルで AvalancheMC は問題なく動いた
- Magboltz (Fortran) の標準出力は抑制できないので、テーブル生成時はログが数百行になる

## ゴール

```
drift-electrons -i Ar_200mbar_5.5MeV.root -v 2000
```

で `Ar_200mbar_5.5MeV_2000V.root` ができ、底面 y = -100 mm に到達した電子ごとの到達位置 (x, z)、到達時刻 t、生成点が得られる。これから「電子がどれだけ底面に届くか」「位置の広がりがどれだけか」を解析する。

## 設計判断

- 電場は一様。E = V / 200 mm、向きは +y。読み出し面 (y = -100 mm) を高電位、カソード (y = +100 mm) を低電位にし、電子は -y に走る。`-v` は電極間電圧の絶対値 [V]
- ガスは MediumMagboltz。組成は Stage 1 と同じ単一ガス、圧力は Stage 1 と同じ値を Torr に換算 (1 mbar = 0.750062 Torr)、温度 293.15 K
- 輸送パラメータ (ドリフト速度、拡散係数) は Magboltz で計算する。電場は 1 点なので、その E だけのテーブル (`SetFieldGrid(E, E, 1, false)`) を生成し `{cachedir}/{gas}_{p}mbar_{E}Vcm.gas` にキャッシュする。あれば `LoadGasFile`、無ければ `GenerateGasTable(10)` して `WriteGasFile` (1 分程度)。cachedir は `-c` で指定、既定はカレントディレクトリの `gasfiles`。1 点のテーブルで AvalancheMC が動かない場合は 3 点 (0.9E, E, 1.1E) にして最終報告に書く
- Garfield++ の単位は cm, ns, V, Torr。座標は Stage 1 と同じ (中心原点、y が鉛直、読み出し面 y = -10 cm)
- 電子数: ヒットごとに n = edep / W。揺らぎは σ = sqrt(F n) のガウスで丸める。W と F は Magboltz が持つ値を使う
- ドリフトは AvalancheMC。拡散あり、距離ステップ 1 mm。一様電場なので解析的に書けるが、Garfield++ を使う方針 (plan の議論) と、将来の非一様電場 (フィールドケージ、GEM) への拡張性を優先する
- 幾何は GeometrySimple + SolidBox (中心原点、半幅 100, 100, 250 mm)。ComponentConstant に medium と電場を設定し、Sensor の領域を箱に合わせる。底面に達した電子は medium の外に出て止まる (status = StatusLeftDriftMedium または StatusLeftDriftArea)
- 電子 1 個ずつドリフトすると 5.5 MeV の α で約 2×10^5 電子/イベント、1 電子 200 ステップなので 1 イベント数秒から数十秒。`-f 割合` で間引き、各電子に重み 1/f を付ける (既定 1)
- 単位: Garfield++ は cm, ns, V。入出力は Stage 1 と同じ mm, ns, MeV に揃える。換算は入出力の境界だけで行う
- 増幅 (GEM, MWPC) と磁場は扱わない。底面に到達した電子の位置と時刻まで

## 必要な Stage 1 側の追加

Stage 2 がガス名と圧力を知る必要がある。ファイル名から読むのは脆いので、Stage 1 に ntuple `run` (1 行) を追加する。

| 列 | 型 | 意味 |
|---|---|---|
| gas | S | `/tpc/gas` の値 |
| pressure | D | `/tpc/pressure` の値 [mbar] |
| hits | I | `/tpc/hits` の値 |

`-g`, `-p` で上書きできるようにはしない (KISS)。`run` が無いファイルはエラー。この追加は TODO/02 の「検証後の追加タスク」3 で行う。

## CLI

```
drift-electrons -i <input.root> -v <volt> [-f <fraction>] [-n <maxEvents>] [-c <cachedir>] [-h]
```

- `-i`, `-v` は必須。欠けていれば usage を stderr に出して終了コード 1
- `-f` の既定は 1 (全電子)。0 < f ≤ 1
- `-n` を指定するとその数のイベントだけ処理する (試運転用)
- `-c` はガステーブルのキャッシュディレクトリ。既定 `gasfiles`。無ければ作る
- 出力名は入力名の `.root` の前に `_{V}V` を付ける。数値は `%g`
- 入力に `hits` が無ければ終了コード 1

## ROOT 出力の構造

ntuple `electrons` (1 行 = 底面に到達した電子 1 個)

| 列 | 型 | 意味 |
|---|---|---|
| eventID | I | Stage 1 のイベント番号 |
| x0, y0, z0 | D | 電子の生成点 [mm] (ヒットのステップ終点) |
| t0 | D | 生成時刻 [ns] |
| x, y, z | D | 到達点 [mm]。y は -100 mm |
| t | D | 到達時刻 [ns] |
| weight | D | この電子が代表する電子数 (1/f) |
| status | I | Garfield++ の終了コード |

ntuple `run` (1 行): gas (S), pressure (D), voltage (D), efield (D) [V/cm], vdrift (D) [cm/us], dl (D), dt (D) [sqrt(cm)], w (D) [eV], fano (D)

Stage 1 の `events` は ROOT の TTree::CloneTree でそのままコピーする (解析で e0 などを参照するため)。

## 期待値の根拠 (Magboltz, 200 mbar, 293.15 K, ncoll = 2 の概算)

Magboltz (Garfield++ 2025.12) を純ガス、200 mbar、293.15 K、ncoll = 2 で走らせた値。統計誤差は数 % あるので設計の目安に使う。実装時のテーブルは ncoll = 10 で作り直す。

| ガス | W [eV] | Fano | E [V/cm] | 電圧 [V] (20 cm) | v_drift [cm/µs] | D_L [√cm] | D_T [√cm] | 10 cm ドリフト後の σ_T [mm] |
|---|---|---|---|---|---|---|---|---|
| He | 41.3 | 0.17 | 50 | 1000 | 0.49 | 0.092 | 0.131 | 4.2 |
| He | | | 100 | 2000 | 0.70 | 0.077 | 0.123 | 3.9 |
| He | | | 200 | 4000 | 1.01 | 0.087 | 0.146 | 4.6 |
| He | | | 500 | 10000 | 2.20 | 0.131 | 0.130 | 4.1 (増幅が始まる: α = 0.024 /cm) |
| Ar | 26.4 | 0.17 | 50 | 1000 | 0.30 | 0.147 | 0.328 | 10.4 |
| Ar | | | 100 | 2000 | 0.34 | 0.122 | 0.314 | 9.9 |
| Ar | | | 200 | 4000 | 0.42 | 0.112 | 0.290 | 9.2 |
| Ar | | | 500 | 10000 | 0.97 | 0.109 | 0.179 | 5.7 |
| CO2 | 33.0 | 0.32 | 50 | 1000 | 0.18 | 0.034 | 0.031 | 1.0 |
| CO2 | | | 100 | 2000 | 0.36 | 0.024 | 0.025 | 0.8 |
| CO2 | | | 200 | 4000 | 0.72 | 0.020 | 0.019 | 0.6 |
| CO2 | | | 500 | 10000 | 2.09 | 0.021 | 0.016 | 0.5 |

- 純 Ar は横拡散が大きく、10 cm で 1 cm 広がる。純 CO2 は冷たいガスで拡散は小さいがドリフトが遅い。混合ガスにすると中間になる
- 200 mm ドリフトの時間は He 100 V/cm で約 29 µs、Ar 100 V/cm で約 58 µs、CO2 100 V/cm で約 55 µs
- 電離電子数の目安: 5.5 MeV の α で He 1.3×10^5、Ar 2.1×10^5、CO2 1.7×10^5。300 keV なら He 7.3×10^3

## 受け入れテスト (案)

### AT2-1 CLI と出力名

- `-i` または `-v` が無い → 終了コード 1
- `-i Ar_200mbar_5.5MeV.root -v 2000` → `Ar_200mbar_5.5MeV_2000V.root`
- `hits` の無い入力 → 終了コード 1、出力は作られない

### AT2-2 1 電子のドリフト (Garfield++ 依存の単体テスト)

Ar 200 mbar、E = 100 V/cm、電子を (0, 0, 0) から 1000 個ドリフトさせる。

- 全電子が y = -100 mm (誤差 1 mm) に到達
- 到達時刻の平均が 100 mm / v_drift の ±5 % 以内
- x の標準偏差が D_T × sqrt(10 cm) の ±15 % 以内 (v_drift, D_T は同じテーブルの値)

### AT2-3 端から端まで

Stage 1 を Ar 200 mbar 5.5 MeV `/tpc/hits true` で 10 イベント走らせ、`-v 2000` で Stage 2 を走らせる。

- 全電子の y が -100 mm の ±0.5 mm 以内
- イベントごとの weight の和が e0 / W の ±5 % 以内
- (t - t0) の平均が (y0 + 100 mm) / v_drift の平均の ±5 % 以内
- `run` の gas, pressure, voltage が入力と CLI の値に一致

### AT2-4 キャッシュ

- 同じガス・圧力・電圧で 2 回目の実行では gas ファイルを生成せず、1 回目より速い

## 単体テスト (GoogleTest)

- CLI 解析、出力名、E = V / 20 cm、mbar → Torr
- 電子数サンプリング: 10^5 回の平均が edep / W の ±1 %、分散が F × 平均の ±10 %

## 実装タスク (TDD の順)

1. (済) Stage 1 に `run` ntuple を追加
2. CMake に Garfield++ (`find_package(Garfield)`, `CMAKE_PREFIX_PATH=/opt/Garfield`, ターゲット `Garfield::Garfield`) と ROOT (`find_package(ROOT)`, `ROOT::RIO ROOT::Tree`) を追加。Stage 2 のターゲット (`drift-electrons` と そのライブラリ) だけがリンクする。Stage 1 のターゲットは変えない
3. 純関数: CLI、出力名、E、換算、電子数サンプリング
4. ガス: MediumMagboltz の生成とキャッシュ
5. 幾何・センサー・AvalancheMC で 1 電子をドリフト (AT2-2)
6. 入力 ROOT の読み込み、ヒットごとの電子生成とドリフト、出力 (AT2-3)
7. AT2-1, AT2-4

## 相談したいこと

- 純 He、純 Ar、純 CO2 はそれぞれ実機のガスと違う (純 Ar は拡散が大きく、純 He は電離電子が少ない)。混合ガス (He/CO2 90/10 など) は MediumMagboltz では `SetComposition("he", 90., "co2", 10.)` で済むが、Stage 1 の材料定義も必要になる。いつ入れるか
- 電圧の想定範囲。E = 100 V/cm なら 2 kV、500 V/cm なら 10 kV
- 底面の読み出し構造 (パッドサイズ) を考慮した解析を Stage 2 の出力に対して行うか、別ステップにするか
