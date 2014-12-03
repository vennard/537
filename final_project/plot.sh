#!/bin/bash

#test graph
FILE=$1
RVALUE=$2

DELAY=0
echo Calculating max delay using file: $FILE and slope $SLOPE:

SLOPE=`echo "$RVALUE / 1000" | bc -l`
echo Slope = $SLOPE

while read line;do
    eval vals=($line)
    X=${vals[0]}
    Y=${vals[1]}
    y1=`echo "$Y / $SLOPE" | bc` 
    echo Y1 is $y1
    d=`echo "$y1 - $X" | bc`
    echo d is $d
    dx=`echo "$d * -1" | bc`
    echo dx is $dx
    if [ $dx -gt $DELAY ];then
        DELAY=$dx
    fi
    let k++
done < $FILE

echo MAX DELAY = $DELAY

gnuplot <<- EOF
    set yrange [0:10]
    set xlabel "ms"
    set ylabel "frame"
    set term svg
    set nokey
    set output "out.svg"
    F(x) = $SLOPE * (x - $DELAY)
    plot "$FILE", F(x) 
EOF



exit 0
