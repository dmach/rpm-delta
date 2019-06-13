#!/bin/sh

# dnf install rpm-devel
gcc rpm_split.c -lrpm -lrpmio -o rpm_split
gcc rpm_join.c -lrpm -lrpmio -o rpm_join
