#!/bin/bash
#
# usage: ./plot.sh <data file>
# 
# outputs:
#   out.svg - graph of data points and slope lines
#   out.txt - data file containing all delay values with associated R values (10->100)
#
# note: may have to give permission using chmod a+x plot.sh

FILE=$1
OUTFILE=$2
SLOPE_ARRAY=()
DELAY_ARRAY=()

echo Calculating max delay using file: $FILE 
echo ECE537 ATeam Project Delay Values > $OUTFILE.txt
echo John Vennard \& Jan Beran >> $OUTFILE.txt
echo Input file: $FILE >> $OUTFILE.txt

# Loop over each R value increment by 10
for i in {10..100..10}; do
    DELAY=0
    # Calculate slope in frame/ms
    SLOPE=`echo "$i / 1000" | bc -l`
    SLOPE_ARRAY+=($SLOPE)
    # calculate Delay = Y/R - X
    while read line;do
        eval vals=($line)
        X=${vals[0]}
        Y=${vals[1]}
        y1=`echo "$Y / $SLOPE" | bc` 
        d=`echo "$y1 - $X" | bc`
        dx=`echo "$d * -1" | bc`
        if [ $dx -gt $DELAY ];then
            DELAY=$dx
        fi
        let k++
    done < $FILE
    echo MAX DELAY with R of $i = $DELAY
    echo R = $i results in max delay = $DELAY ms >> $OUTFILE.txt
    DELAY_ARRAY+=($DELAY)
done

gnuplot <<- EOF
    set yrange [0:*]
    set xlabel "ms"
    set ylabel "frame"
    set term svg 
    set nokey
    set output "$OUTFILE.svg"
    F1(x) = ${SLOPE_ARRAY[0]} * (x - ${DELAY_ARRAY[0]})
    F2(x) = ${SLOPE_ARRAY[1]} * (x - ${DELAY_ARRAY[1]})
    F3(x) = ${SLOPE_ARRAY[2]} * (x - ${DELAY_ARRAY[2]})
    F4(x) = ${SLOPE_ARRAY[3]} * (x - ${DELAY_ARRAY[3]})
    F5(x) = ${SLOPE_ARRAY[4]} * (x - ${DELAY_ARRAY[4]})
    F6(x) = ${SLOPE_ARRAY[5]} * (x - ${DELAY_ARRAY[5]})
    F7(x) = ${SLOPE_ARRAY[6]} * (x - ${DELAY_ARRAY[6]})
    F8(x) = ${SLOPE_ARRAY[7]} * (x - ${DELAY_ARRAY[7]})
    F9(x) = ${SLOPE_ARRAY[8]} * (x - ${DELAY_ARRAY[8]})
    plot "$FILE", F1(x), F2(x), F3(x), F4(x), F5(x), F6(x), F7(x), F8(x), F9(x) 
EOF
echo done
exit 0
