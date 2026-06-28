#!/usr/bin/env bash
rm -rf slpp_unit
$1 pkg1.sv -fileunit -nodb -nobuiltin -nocomp -nohash -d cache
$1 pkg2.sv -fileunit -nodb -nobuiltin -nocomp -nohash -d cache
$1 pkg1.sv pkg2.sv -fileunit -nohash -d db -nobuiltin -d cache
