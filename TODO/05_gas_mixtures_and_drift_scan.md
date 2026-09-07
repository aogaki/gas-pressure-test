# 05 混合ガスとドリフトのスキャン運用

前提: TODO/01〜04。状態: 実装済み (2026-09-07)。AT-M1〜M3, AT-D1, D2 合格、全 83 テスト合格。

検証結果 (発注者による):

- AT-M2: 5.5 MeV、1013.25 mbar の平均経路長は He-90-CO2-10 で 135.9 mm (加法則の推定 134.2 の 1.013 倍)、Ar-90-CO2-10 で 43.7 mm (42.4 の 1.031 倍)。混合材料は base material を持たず Geant4 の元素ベースの阻止能になるため、ASTAR 直読みの単一ガスより差が大きい。許容 ±5 % 内だが Ar/CO2 は余裕が小さい
- AT-M3: He-90-CO2-10、200 mbar、0.3 MeV、2000 V で W = 40.47 eV、Fano = 0.185、電子数 7414/イベント (e0/W = 7413)、到達時刻は期待値の 0.9999 倍
- He-90-CO2-10、200 mbar の輸送パラメータ (Magboltz): 50 V/cm で v = 0.90 cm/µs、100 V/cm で v = 1.60 cm/µs、D_T = 0.040 √cm。純 He (0.70 cm/µs、0.123 √cm) より速く拡散が小さい
- 既知の穴: 成分名の重複 (`He-50-Ar-40-He-10`) を弾いていない。TODO/06 の作業で直す

## 決定

- 混合ガスを Stage 1 (Geant4 材料) と Stage 2 (Magboltz 組成) の両方に入れる
- Stage 2 の電圧は 1000, 1200, 1400, 1600, 2000 V (20 cm ギャップで 50〜100 V/cm)
- 圧力の下限は 50 mbar (scan.sh の既定のまま)

## ガス指定の書式

`名前-割合-名前-割合...`。例: `He-90-CO2-10`, `Ar-90-CO2-10`, `He-70-CO2-30`。

- 割合は体積 (モル) パーセント。合計が 100 でなければエラー。各割合は 0 より大きい。小数も可 (`He-92.5-CO2-7.5`)
- 成分は 1 から 6 個 (Magboltz の上限)。単一ガスは今まで通り `He` と書ける (`He-100` と同じ)
- 成分名は He, Ar, CO2 のみ。順序はそのまま名前に使う (正規化しない)
- この文字列を `/tpc/gas`、ファイル名、`run.gas`、Stage 2 の組成、ガステーブルのキャッシュ名すべてで使う。区切りにコロンやスラッシュを使わないのは、ROOT のファイル名と Unix のパスで安全にするため

## Stage 1 の材料

- 密度: 理想気体の分圧の和。ρ(P) = (P / 1013.25) × Σ (f_i / 100) ρ_i,NIST
- CO2 の ρ_NIST は実在気体 (1 atm) の値なので、これを圧力に線形にスケールすると理想気体より 0.68 % 大きい。低圧では理想気体に近づくため、この分だけ Stage 1 の密度が Stage 2 (Magboltz、理想気体) と食い違う (He, Ar は無視できる大きさ)
- 単一ガスは今まで通り `BuildMaterialWithNewDensity` (base material 経由で ASTAR が使われる)
- 混合ガスは `new G4Material(name, density, ncomponents, kStateGas, 293.15 K, P)` に NIST 材料を `AddMaterial(nist_i, w_i)` で加える。質量分率は w_i = f_i ρ_i,NIST / Σ f_j ρ_j,NIST (理想気体なので分子量の表は不要)
- 材料名は `{gas}_{p}mbar` (今まで通り)。同じ名前で 2 回作らない

期待値 (NIST ASTAR の質量飛程に飛程加法則 1/R = Σ w_i / R_i を適用した近似。数 % の誤差がある):

| ガス | P [mbar] | ρ [g/cm3] | w (質量分率) | 5.5 MeV の期待経路長 [mm] |
|---|---|---|---|---|
| He-90-CO2-10 | 1013.25 | 3.3390e-4 | He 0.4483, CO2 0.5517 | 134.2 |
| Ar-90-CO2-10 | 1013.25 | 1.6800e-3 | Ar 0.8904, CO2 0.1096 | 42.4 |
| He-90-CO2-10 | 200 | 6.5907e-5 | 同上 | 0.3 MeV で 43.3 |

## Stage 2 の組成

- `run.gas` を解析して `MediumMagboltz::SetComposition(name1, f1, name2, f2, ...)` に渡す。Magboltz の名前は he, ar, co2
- W と Fano は Magboltz の値をそのまま使う (He-90-CO2-10 で W = 40.47 eV, Fano = 0.185 を確認済み)
- Magboltz の W と Fano は成分の体積分率の線形平均で、W_mix = Σ (f_i / 100) W_i, Fano_mix = Σ (f_i / 100) Fano_i。He-90-CO2-10 の 40.47 eV は 0.9 × 41.3 + 0.1 × 33.0、0.185 は 0.9 × 0.17 + 0.1 × 0.32 の内訳。エネルギー分配 (CO2 の阻止能が大きい分だけ CO2 側に多く落ちる) と Penning (Jesse) 効果を含まないので、He 混合の電離電子数はこの W から出した値が下限になる (実際はもっと多い)
- キャッシュ名 `{gas}_{p}mbar_{E}Vcm.gas` はそのまま (ダッシュ入り)
- ガステーブルの書き込みは一時ファイルに書いてから rename する (並列実行で同じテーブルを同時に作っても壊れないように)

## scan.sh の追加

- `-H` を付けるとマクロの `/tpc/hits` が true になる (既定は false)。Stage 2 に渡すランはこれで作る

## scripts/drift_scan.sh

```
scripts/drift_scan.sh -o <outdir> -v "1000 1200 1400 1600 2000" [-f 0.1] [-j 14] [-x <drift-electrons>] [-c <cachedir>] [-d]
```

- `<outdir>/*.root` のうち Stage 1 の出力 (名前が `V.root` で終わらないもの) すべてに対し、各電圧で `drift-electrons -i <file> -v <V> -f <f> -c <cachedir>` を走らせる
- ログは `<outdir>/logs/{name}_{V}V.log`。`-c` の既定は `<outdir>/gasfiles`。`-x` の既定は `build/drift-electrons`
- 並列は `xargs -P`。失敗したランの報告と終了コードは scan.sh と同じ流儀
- `-d` はコマンドを表示するだけ

## 受け入れテスト

### AT-M1 書式の解析 (単体テスト)

- `"He"` → [(He, 100)]、`"He-90-CO2-10"` → [(He, 90), (CO2, 10)]、`"He-92.5-CO2-7.5"` も可
- エラー (std::invalid_argument): `"He-90"` (要素数が奇数)、`"He-90-CO2-20"` (合計 110)、`"He-0-CO2-100"` (0 の割合)、`"Xe-100"` (未知)、`"He-CO2-90-10"` (数字でない)
- `GasDensity("He-90-CO2-10", 1013.25)` = 3.3390e-4 g/cm3 (相対誤差 1e-3)、200 mbar で 6.5907e-5
- (Geant4 依存) `BuildGasMaterial("He-90-CO2-10", 200)`: 密度 6.5907e-5、元素数 3 (He, C, O)、kStateGas、2 回呼ぶと同じポインタ

### AT-M2 混合ガスの飛程 (Stage 1)

- He-90-CO2-10、1013.25 mbar、5.5 MeV、1000 イベント: 全 exited = 0、trackLength の平均が 134.2 mm の ±5 % 以内
- Ar-90-CO2-10、1013.25 mbar、5.5 MeV、1000 イベント: 全 exited = 0、平均が 42.4 mm の ±5 % 以内
- 自動命名が `He-90-CO2-10_1013.25mbar_5.5MeV.root` になる

### AT-M3 混合ガスのドリフト (Stage 2)

- Stage 1: He-90-CO2-10、200 mbar、0.3 MeV、hits true、10 イベント
- Stage 2: `-v 2000 -f 0.1` → `run.gas` = "He-90-CO2-10"、`run.w` が 40.47 の ±0.5 %、`run.fano` が 0.185 の ±3 %、AT2-3 と同じ判定 (電子数が e0/W の ±5 %、到達時刻)
- 既存の check_drift.C を引数で使い回せるなら使う

### AT-D1 scan.sh -H

- `scan.sh -H -d ...` が作るマクロに `/tpc/hits true` が入る。`-H` なしなら false

### AT-D2 drift_scan.sh

- `-d` で 2 電圧を指定 → コマンドが 2 つ表示され、ROOT ファイルはできない
- Stage 1 の hits true の出力 1 つ (Ar 200 mbar 5.5 MeV 10 イベント) があるディレクトリに `-v "2000" -f 0.01 -c <既存のキャッシュ>` → `..._2000V.root` ができ、`run.voltage` = 2000、ログがある。キャッシュは AT2-4 の共有テーブルを使い Magboltz を走らせない

## 実装タスク (TDD の順)

1. `ParseGasSpec` (純関数) と AT-M1 の単体テスト。既存の `GasNistName` / `GasDensity` を混合対応に置き換える (単一ガスの結果は変えない)
2. `BuildGasMaterial` の混合対応
3. AT-M2
4. Stage 2: 組成の設定、キャッシュの rename、AT-M3
5. scan.sh の `-H`、drift_scan.sh、AT-D1, AT-D2
6. TODO/01 の「ガス」節と CLAUDE.md の規約に書式を追記
