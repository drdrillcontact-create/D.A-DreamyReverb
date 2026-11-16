#!/usr/bin/env bash
# Puts the telling lines of a log on the workflow run page as an annotation, so that the outcome
# of a build or validation step can be read without opening the full log.
#
#   annotate.sh <title> <log file> [<grep -E pattern>] [<level>]
#
# Defaults: every line, level "error". The last 60 selected lines are shown - or fewer: GitHub
# cuts a note off after 4096 characters, so only as many of the last lines as fit into one.
title="$1"
log="$2"
pattern="${3:-.}"
level="${4:-error}"

body="$(tr -d '\r' < "$log" | grep -aE "$pattern" | tail -n 60 | cut -c1-220 \
          | awk '{ line[NR] = $0 }
                 END { first = NR + 1; total = 0
                       for (i = NR; i >= 1; --i) { total += length(line[i]) + 1; if (total > 3900) break; first = i }
                       for (i = first; i <= NR; ++i) print line[i] }' \
          | sed -e 's/%/%25/g' | awk '{ printf "%s%%0A", $0 }')"
echo "::${level} title=${title}::${body:-no matching lines in ${log}}"
