#!/bin/sh

alic "-DBLUES -DSYNC_MINUTES=1 \"-DPRODUCT_UID=\"$(cat /var/tmp/blues)\"\"" && aliu && alim
