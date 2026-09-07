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

mkdir -p "$outdir/logs"
rm -f "$outdir/.failed"

# The inputs are the ROOT files of Stage 1, that is every *.root of outdir
# except the outputs of the later stages: Stage 2 writes "_{V}V.root" (a digit
# in front of the V, so that the "MeV" of a Stage 1 name is not mistaken for
# one) and Stage 3 writes "_readout.root". In normal mode
# each (input, voltage) pair is printed to stdout, building the run list
# consumed by xargs below; in dry-run mode the would-be command goes to
# stderr instead so stdout is not mixed into the run list.
runlist=$(
  for file in "$outdir"/*.root; do
    [ -e "$file" ] || continue
    name=$(basename "$file" .root)
    case $name in (*[0-9]V | *_readout) continue ;; esac
    for v in $voltages; do
      if [ "$dry_run" -eq 1 ]; then
        echo "(cd $outdir && $exe -i $name.root -v $v -f $fraction" \
             "-c $cachedir > logs/${name}_${v}V.log 2>&1)" >&2
      else
        echo "$name $v"
      fi
    done
  done
)

[ "$dry_run" -eq 1 ] && exit 0

# Run every pair, up to $jobs at a time. Each run's stdout/stderr goes to its
# own log; a run that fails records its name in outdir/.failed instead of
# stopping the others (xargs's own exit status is not reliable across
# implementations, so it is deliberately ignored here).
if [ -n "$runlist" ]; then
  printf '%s\n' "$runlist" | xargs -P "$jobs" -I{} sh -c '
    exe=$2
    outdir=$3
    fraction=$4
    cachedir=$5
    set -- $1
    name=$1
    v=$2
    cd "$outdir" || exit 1
    if "$exe" -i "$name.root" -v "$v" -f "$fraction" -c "$cachedir" \
        >"logs/${name}_${v}V.log" 2>&1; then
      exit 0
    fi
    echo "${name}_${v}V" >> .failed
  ' _ {} "$exe" "$outdir" "$fraction" "$cachedir" || true
else
  echo "drift_scan.sh: no Stage 1 ROOT file in $outdir" >&2
fi

if [ -s "$outdir/.failed" ]; then
  echo "drift_scan.sh: failed run(s):" >&2
  cat "$outdir/.failed" >&2
  rm -f "$outdir/.failed"
  exit 1
fi
exit 0
