#!/bin/sh
# Runs a grid of Stage 1 macros over gas x pressure x energy, in parallel,
# and collects the ROOT outputs and per-run logs under one directory.
# See TODO/04_scan.md.
set -eu

usage() {
  echo 'usage: scan.sh -o <outdir> [-g "He Ar CO2"] [-p "50 100 150 200"]' >&2
  echo '                [-e "0.3 0.5 1 ... 10"] [-n 1000000] [-j 14] [-x <exe>] [-d]' >&2
  exit 1
}

script_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
project_root=$(CDPATH= cd "$script_dir/.." && pwd)

outdir=""
gases="He Ar CO2"
pressures="50 100 150 200"
energies="0.3 0.5 1 1.5 2 2.5 3 3.5 4 4.5 5 5.5 6 6.5 7 7.5 8 8.5 9 9.5 10"
nevents=1000000
jobs=14
exe="$project_root/build/gas-pressure-test"
dry_run=0

while getopts "o:g:p:e:n:j:x:d" opt; do
  case $opt in
    o) outdir=$OPTARG ;;
    g) gases=$OPTARG ;;
    p) pressures=$OPTARG ;;
    e) energies=$OPTARG ;;
    n) nevents=$OPTARG ;;
    j) jobs=$OPTARG ;;
    x) exe=$OPTARG ;;
    d) dry_run=1 ;;
    *) usage ;;
  esac
done
[ -n "$outdir" ] || usage

# Make exe absolute: the run wrapper below cds into outdir, so a relative
# path given via -x would otherwise stop resolving correctly.
case $exe in
  /*) ;;
  *) exe="$(pwd)/$exe" ;;
esac

mkdir -p "$outdir/macros" "$outdir/logs"
rm -f "$outdir/.failed"

# Generate one macro per (gas, pressure, energy) condition. In normal mode
# each condition's name is also printed to stdout, building the run list
# consumed by xargs below; in dry-run mode the would-be command goes to
# stderr instead so stdout is not mixed into the run list.
runlist=$(
  i=0
  for gas in $gases; do
    for p in $pressures; do
      for e in $energies; do
        i=$((i + 1))
        name="${gas}_${p}mbar_${e}MeV"
        cat > "$outdir/macros/$name.mac" <<EOF
/tpc/gas $gas
/tpc/pressure $p
/tpc/hits false
/run/initialize
/gps/energy $e MeV
/analysis/setFileName $name.root
/random/setSeeds $i $i
/run/beamOn $nevents
EOF
        if [ "$dry_run" -eq 1 ]; then
          echo "(cd $outdir && $exe macros/$name.mac > logs/$name.log 2>&1)" >&2
        else
          echo "$name"
        fi
      done
    done
  done
)

[ "$dry_run" -eq 1 ] && exit 0

# Run every condition, up to $jobs at a time. Each run's stdout/stderr goes
# to its own log; a run that fails records its name in outdir/.failed
# instead of stopping the others (xargs's own exit status is not reliable
# across implementations, so it is deliberately ignored here).
if [ -n "$runlist" ]; then
  printf '%s\n' "$runlist" | xargs -P "$jobs" -I{} sh -c '
    name=$1
    exe=$2
    cd "$3" || exit 1
    if "$exe" "macros/$name.mac" >"logs/$name.log" 2>&1; then
      exit 0
    fi
    echo "$name" >> .failed
  ' _ {} "$exe" "$outdir" || true
fi

if [ -s "$outdir/.failed" ]; then
  echo "scan.sh: failed run(s):" >&2
  cat "$outdir/.failed" >&2
  rm -f "$outdir/.failed"
  exit 1
fi
exit 0
