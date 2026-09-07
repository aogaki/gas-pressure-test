# 04 エネルギースキャンの運用

前提: TODO/01, TODO/02。Stage 1 を格子状の条件で並列実行し、結果を表にまとめる。
状態: 実装済み (2026-09-07)。AT-S1〜S4 合格。

デモ: `scripts/scan.sh -o scans/demo_200mbar_100k -p "200" -n 100000 -j 14` は 63 ラン (630 万イベント) を 18 秒で完了した。200 mbar では Ar は 9.5 MeV 以上、He は 3.5 MeV 以上で突き抜け、CO2 は 10 MeV でも 360 mm で止まる。

## 決定

- エネルギー: 0.3 MeV と、0.5 から 10 MeV の 0.5 刻み (合計 21 点)
- 1 ラン 1M イベント、`/tpc/hits false`
- ガス: He, Ar, CO2。圧力: 上限 200 mbar、下限は未定なので引数で与える (既定は 50 100 150 200)
- MT は使わず、プロセス並列 (既定 14)
- 速度: 15900 イベント/秒なので 1 ランは約 63 秒。315 ランで約 25 分

## scripts/scan.sh

```
scripts/scan.sh -o <outdir> [-g "He Ar CO2"] [-p "50 100 150 200"] [-e "0.3 0.5 1 ... 10"] [-n 1000000] [-j 14] [-x <exe>] [-d]
```

- POSIX sh。依存は `xargs -P` だけ
- `-o` は必須。`<outdir>/macros/`, `<outdir>/logs/` を作り、ROOT ファイルは `<outdir>/` 直下に置く
- 条件ごとにマクロ `{gas}_{p}mbar_{E}MeV.mac` を生成する。中身は次の通り。数値は引数の文字列をそのまま使う (`1.0` なら `1.0`)。`/analysis/setFileName` を明示するので、マクロ名・ログ名・ROOT 名が必ず一致する

```
/tpc/gas {gas}
/tpc/pressure {p}
/tpc/hits false
/run/initialize
/gps/energy {E} MeV
/analysis/setFileName {gas}_{p}mbar_{E}MeV.root
/random/setSeeds {i} {i}
/run/beamOn {n}
```

- `{i}` は 1 から始まるラン番号。ランごとに違うシードにする
- 実行は `xargs -P <j>` で、各ランの stdout/stderr を `logs/{name}.log` に書く。作業ディレクトリは `<outdir>`
- `-x` の既定は `build/gas-pressure-test` (スクリプトの位置からの相対でプロジェクトルートを求める)
- `-d` (dry run) はマクロを生成してコマンドを表示するだけで実行しない
- 既存の出力は黙って上書き
- 終了コードは、1 つでもランが失敗すれば 0 以外。失敗したランの名前を最後に表示する

## scripts/range_summary.C

```
root -l -b -q 'scripts/range_summary.C("<outdir>")'
```

- `<outdir>/*.root` を全部読み、`run` ntuple からガスと圧力、`events` からエネルギー (e0 の平均) と飛程を集計して CSV を stdout に出す
- 列: gas, pressure_mbar, energy_MeV, events, mean_trackLength_mm, sigma_trackLength_mm, mean_projected_mm, exited_fraction, mean_eExit_MeV
- gas, pressure, energy の順にソート
- 突き抜けたイベントも trackLength の平均に含める (exited_fraction を見れば分かる)。exited_fraction が 0 でない行では飛程の平均は「ガス長で切られた値」なので注意書きを README 相当のコメントに書く

## 受け入れテスト (CTest, ラベル acceptance)

- AT-S1: `scan.sh -d -o <tmp> -g "He" -p "200" -e "0.3 1.0" -n 10` → `macros/He_200mbar_0.3MeV.mac` と `macros/He_200mbar_1.0MeV.mac` ができ、中身が上のテンプレート通り (シードは 1 と 2)。ROOT ファイルはできない
- AT-S2: 同じ条件で `-d` なし、`-j 2` → `He_200mbar_0.3MeV.root` と `He_200mbar_1.0MeV.root` ができ、`events` が 10 エントリ、`run.pressure` が 200。`logs/` に 2 つのログ
- AT-S3: AT-S2 の出力に `range_summary.C` を走らせ、2 行の CSV が出て、energy_MeV が 0.3 と 1.0、events が 10、mean_trackLength_mm が 0 より大きい
- AT-S4: 存在しない実行ファイルを `-x` に与える → 終了コード 0 以外
