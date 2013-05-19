#!/bin/sh -x

file=${1#*sources/}

pushd "sources" > /dev/null
wiggle --replace $file $file.rej
vimdiff $file.orig $file
echo "sources/${file}.orig vs. sources/${file} -> patches/${file}.diff"
diff -du $file.orig $file | sed '1s/\.orig//' >$file.diff
popd > /dev/null

mv -i "sources/$file.diff" "patches/$file.diff"
