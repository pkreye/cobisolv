#!/bin/bash

usage() { echo "Usage: $0 [-f <string>] [-p <num>] [-g <num>] [-i <num>] [-s <num>]" 1>&2; exit 1; }

initialSearchFactorMax=0
globalSearchFactorMax=0
initStep=50
globalStep=50
filePrefix="bqp1000_"

while getopts ":f:p:g:s:i:" o; do
    case "${o}" in
        f)
            filePrefix="$OPTARG"
            ;;
        p)
            initialSearchFactorMax=$((OPTARG))
            ;;
        g)
            globalSearchFactorMax=$((OPTARG))
            ;;
        i)
            initStep=$((OPTARG))
            ;;
        s)
            globalStep=$((OPTARG))
            ;;
        *)
            usage
            ;;
    esac
done

echo "Args: initialSearchFactorMax=" $initialSearchFactorMax ", globalSearchFactorMax=" $globalSearchFactorMax ", step=" $step
echo "--"

cmd="../build/qbsolv -i"

numProbs=10

for probNum in $(eval echo "{1..$numProbs}");
do
    for initialSearch in $(eval echo "{0..$initialSearchFactorMax..$initStep}")
    do
        # echo "initial search factor="$initialSearch
        for globalSearch in $(eval echo "{0..$globalSearchFactorMax..$globalStep}")
        do
            # echo "global search factor="$globalSearch
            infile="./qubos/$filePrefix$probNum.qubo";
            outfile="$filePrefix$probNum.data";
            cmd="../build/qbsolv -p $initialSearch -g $globalSearch -i $infile >> $outfile";
            echo $cmd;
            eval $cmd
        done
    done
done
