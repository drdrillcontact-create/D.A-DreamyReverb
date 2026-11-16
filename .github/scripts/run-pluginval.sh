#!/usr/bin/env bash
# Runs pluginval on a plug-in, once for every random seed given, and reports on the run page.
#
# pluginval loads the plug-in the way a host does and exercises it: parameters, state, programs,
# bus layouts, the editor, odd buffer sizes and sample rates, allocations on the audio thread.
# Much of that is done with random values, so the seed decides what exactly gets tried. Fixed
# seeds make a run repeatable; a failure can be replayed with the seed named in its report:
#
#   pluginval --strictness-level 10 --random-seed <seed> --validate <plug-in>
#
# Usage: run-pluginval.sh <pluginval executable> <plug-in> <label> <seed> [<seed> ...]
set -o pipefail
here="$(cd "$(dirname "$0")" && pwd)"
pluginval="$1"
plugin="$2"
label="$3"
shift 3

if [ $# -eq 0 ]; then
  echo "::error title=pluginval was given no random seed::Set PLUGINVAL_SEEDS in the workflow."
  exit 1
fi

failed=0
: > pluginval-summary.log

for seed in "$@"; do
  # pluginval accepts a decimal number, or hexadecimal in lower case with 0x in front
  if ! [[ "$seed" =~ ^(0x[0-9a-f]+|[0-9]+)$ ]]; then
    echo "::error title=Not a random seed for pluginval::\"$seed\" - use a number such as 7 or 0x1f (lower case)."
    exit 1
  fi

  log="pluginval-$seed.log"
  echo "::group::pluginval with random seed $seed"
  "$pluginval" --strictness-level 10 --random-seed "$seed" --verbose --validate "$plugin" 2>&1 | tee "$log"
  status=$?
  echo "::endgroup::"

  took="$(tr -d '\r' < "$log" | grep -a 'Time taken to run all tests' | tail -n 1 | sed -e 's/.*: *//')"

  # A pass needs pluginval's own verdict as well, in case it ever exits cleanly without testing
  if [ "$status" -eq 0 ] && tr -d '\r' < "$log" | grep -aq '^SUCCESS'; then
    echo "seed $seed: passed${took:+ in $took}" >> pluginval-summary.log
    continue
  fi

  failed=1
  echo "seed $seed: FAILED" >> pluginval-summary.log

  # Which tests failed, then the lines that led up to each failure
  {
    tr -d '\r' < "$log" | awk '/Starting tests in/ { section = $0 } /!!!/ { print section; print "  " $0 }' | uniq
    echo "--"
    if grep -aq '!!!' "$log"; then
      grep -a -B 14 -A 2 '!!!' "$log" | head -n 44
    else
      tail -n 30 "$log"
    fi
  } > "pluginval-$seed-failures.log"

  bash "$here/annotate.sh" "pluginval rejected the $label (random seed $seed)" "pluginval-$seed-failures.log"
done

{
  tr -d '\r' < "pluginval-$1.log" | grep -a -m 1 'Strictness level'
  cat pluginval-summary.log
} > pluginval-report.log

if [ "$failed" -ne 0 ]; then
  bash "$here/annotate.sh" "pluginval: $label" pluginval-report.log . warning
  exit 1
fi

bash "$here/annotate.sh" "pluginval passed the $label" pluginval-report.log . notice
