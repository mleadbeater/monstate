#!/bin/sh
[ $1 = "-q" ] && shift
/bin/md5sum $1 | cut -c1-32

