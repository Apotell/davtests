#!/usr/bin/env bash
$1 -init
$1 pkg1.sv pkg2.sv -sepcomp -nohash
$1 top.sv -sepcomp -nohash
$1 -link -d db -nobuiltin -d cache -nohash

