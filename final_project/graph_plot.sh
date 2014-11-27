#!/bin/bash

# Generates a graph of received packets
#
# Jan Beran

# delete old graph
rm -f $2

# plot the graph
echo "\
set terminal png linewidth 2
set output '$2'
set nokey
set title 'Packet rx rate'
set xlabel 'Time [ms]'
set ylabel 'SEQ number'
plot '$1' with lines" \
| gnuplot

# delete data file
# rm -f $1

exit 0
