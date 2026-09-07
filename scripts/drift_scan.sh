#!/bin/sh
# Runs Stage 2 (drift-electrons) on every Stage 1 output of a scan directory,
# once per drift voltage, in parallel.
# See TODO/05_gas_mixtures_and_drift_scan.md.
set -eu

usage() {
  echo 'usage: drift_scan.sh -o <outdir> -v "1000 1200 1400 1600 2000"' >&2
  echo '                     [-f 0.1] [-j 14] [-x <drift-electrons>]' >&2
  echo '                     [-c <cachedir>] [-d]' >&2
  exit 1
}

script_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
project_root=$(CDPATH= cd "$script_dir/.." && pwd)

outdir=""
voltages=""
fraction=0.1
jobs=14
exe="$project_root/build/drift-electrons"
cachedir=""
dry_run=0

while getopts "o:v:f:j:x:c:d" opt; do
  case $opt in
    o) outdir=$OPTARG ;;
    v) voltages=$OPTARG ;;
    f) fraction=$OPTARG ;;
    j) jobs=$OPTARG ;;
    x) exe=$OPTARG ;;
    c) cachedir=$OPTARG ;;
    d) dry_run=1 ;;
    *) usage ;;
  esac
done
[ -n "$outdir" ] || usage
[ -n "$voltages" ] || usage

# The run wrapper below cds into outdir, so the executable and the gas table
# cache have to be absolute paths. The default cache is <outdir>/gasfiles.
[ -n "$cachedir" ] || cachedir="$outdir/gasfiles"
case $exe in
  /*) ;;
  *) exe="$(pwd)/$exe" ;;
esac
case $cachedir in
  /*) ;;
  *) cachedir="$(pwd)/$cachedir" ;;
esac

# The run wrapper below cds into outdir. A directory that cannot be entered has
# to stop the script here, because from inside xargs the failure could not be
# recorded: .failed lives in outdir.
(CDPATH= cd "$outdir") || usage

if [ "$dry_run" -eq 0 ]; then
  mkdir -p "$outdir/logs"
  rm -f "$outdir/.failed"
fi

# The inputs are the ROOT files of Stage 1, that is every *.root of outdir
# except the outputs of the later stages: Stage 2 writes "_{V}V.root" (a digit
# in front of the V, so that the "MeV" of a Stage 1 name is not mistaken for
# one), Stage 3 "_readout.root", Stage 4 "_raw.root", and the analysis macros
# of external/tpcdaq-macros "_uvw.root" and "_tracks.root".
names=$(
  for file in "$outdir"/*.root; do
    [ -e "$file" ] || continue
    name=$(basename "$file" .root)
    case $name in
      (*[0-9]V | *_readout | *_raw | *_uvw | *_tracks) continue ;;
    esac
    printf '%s\n' "$name"
  done
)

if [ -z "$names" ]; then
  echo "drift_scan.sh: no Stage 1 ROOT file in $outdir" >&2
  exit 0
fi

# One pass per voltage. The names go through xargs one per line (so that a
# blank in a file name stays part of the name), the voltage is a fixed
# argument of the wrapper. Each run's stdout/stderr goes to its own log; a run
# that fails records its name in outdir/.failed instead of stopping the others
# (xargs's own exit status is not reliable across implementations, so it is
# deliberately ignored here).
for v in $voltages; do
  if [ "$dry_run" -eq 1 ]; then
    printf '%s\n' "$names" | while IFS= read -r name; do
      echo "(cd $outdir && $exe -i '$name.root' -v $v -f $fraction" \
           "-c $cachedir > 'logs/${name}_${v}V.log' 2>&1)" >&2
    done
    continue
  fi
  printf '%s\n' "$names" | xargs -P "$jobs" -I{} sh -c '
    name=$1
    exe=$2
    outdir=$3
    fraction=$4
    cachedir=$5
    v=$6
    cd "$outdir" || exit 1
    if "$exe" -i "$name.root" -v "$v" -f "$fraction" -c "$cachedir" \
        >"logs/${name}_${v}V.log" 2>&1; then
      exit 0
    fi
    echo "${name}_${v}V" >> .failed
  ' _ {} "$exe" "$outdir" "$fraction" "$cachedir" "$v" || true
done

if [ "$dry_run" -eq 1 ]; then
  exit 0
fi

if [ -s "$outdir/.failed" ]; then
  echo "drift_scan.sh: failed run(s):" >&2
  cat "$outdir/.failed" >&2
  rm -f "$outdir/.failed"
  exit 1
fi
exit 0
