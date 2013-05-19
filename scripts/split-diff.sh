#!/bin/sh -e

# 1) Splits a patch file into smaller patches - one for each file.
# 2) Build a directory structure correspoding to paths in patch headers.
# 3) Copies smaller patches into newly created directories.
# 4) If a patch introduces a completely new file, it will be put into directory
#    as a regular file and not diff file.

[ $# == 1 ] || { echo "Error: expected single patch file as an argument."; false; }
[ -f $1 ] || { echo "Error: no such file '$1'."; false; }

path="$1"

splitdiff -a "${path}" >/dev/null

for p in ${path}.part*; do
  file=`sed -E -n -e '1s/---[[:space:]]+([^[:space:]]+).*/\1/p' ${p}`
  origsize=`sed -E -n -e '3s/@@ ([^[:space:]]+) .*/\1/p' ${p}`
  mkdir -p `dirname ${file}`
  if [ "${origsize}" == "-0,0" ]; then
    sed -E -n -e '3,$s/^\+(.*)/\1/p' ${p} >${file}
  else
    cp ${p} ${file}.diff
  fi
  rm ${p}
done
