#!/bin/sh

./a.out 7
mv out/out.mp4 out/yt.mp4
./a.out 8 1 "$1"
mv out/out.mp4 out/ig.mp4
