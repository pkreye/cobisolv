#!/bin/bash

usage() { echo "Usage: $0 [-f <string>] [-p <num>] [-g <num>] [-i <num>] [-s <num>] [-r <num>]" 1>&2; exit 1; }

initialSearchFactorMax=0;
globalSearchFactorMax=0;
initStep=50;
globalStep=50;
filePrefix="bqp1000_";

seed=17932241798878;

while getopts ":f:p:g:s:i:r:" o; do
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
        r)
            seed=$((OPTARG))
            ;;
        *)
            usage
            ;;
    esac
done

echo "Args: initialSearchFactorMax=" $initialSearchFactorMax ", globalSearchFactorMax=" $globalSearchFactorMax ", step=" $step
echo "--"

numProbs=10

for probNum in $(eval echo "{1..$numProbs}");
do
    infile="./qubos/$filePrefix$probNum.qubo";
    outfile1="${filePrefix}${probNum}_par2_loop_block.data";
    outfile2="${filePrefix}${probNum}_par2_loop_for.data";

    for initialSearch in $(eval echo "{0..$initialSearchFactorMax..$initStep}")
    do
        # echo "initial search factor="$initialSearch
        for globalSearch in $(eval echo "{0..$globalSearchFactorMax..$globalStep}")
        do
            cmd="../build/qbsolv -p $initialSearch -g $globalSearch -i $infile >> $outfile1";
            echo $cmd;
            eval $cmd;

            # echo "global search factor="$globalSearch

            par="../build/qbsolv_par2_loop -p $initialSearch -g $globalSearch -i $infile >> $outfile2";
            echo $par;
            eval $par;
        done
    done
done
