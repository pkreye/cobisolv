#!/bin/bash

numProbs=10
probName="bqp1000"

outfile_seq="./default_seq_${probName}.data";
outfile_par2_reduction="./default_par2_reduction_${probName}.data";
outfile_par2_critical="./default_par2_critical_${probName}.data";
outfile_par4_reduction="./default_par4_reduction_${probName}.data";
outfile_par4_critical="./default_par4_critical_${probName}.data";
outfile_par2_loop="./default_par2_loop_${probName}.data";
outfile_par2_loop_block="./default_par2_loop_${probName}_block.data";
outfile_par2_loop_for="./default_par2_loop_${probName}_for.data";

for i in $(seq 1 10); do
    for probNum in $(eval echo "{1..$numProbs}");
    do
        infile="./qubos/${probName}_${probNum}.qubo";

        # seq="../build/qbsolv_seq -p 6500 -g 1700 -i $infile >> $outfile_seq";
        # echo $seq;
        # eval $seq;

        # par2="../build/qbsolv_par2_reduction -p 6500 -g 1700 -i $infile >> $outfile_par2_reduction";
        # echo $par2;
        # eval $par2;

        # par2crit="../build/qbsolv_par2_critical -p 6500 -g 1700 -i $infile >> $outfile_par2_critical";
        # echo $par2crit;
        # eval $par2crit;

        # par2loop="../build/qbsolv_par2_loop -p 6500 -g 1700 -i $infile >> $outfile_par2_loop";
        # echo $par2loop;
        # eval $par2loop;

        par2loopf="../build/qbsolv_par2_loop_for -p 6500 -g 1700 -i $infile >> $outfile_par2_loop_for";
        echo $par2loopf;
        eval $par2loopf;

        par2loopb="../build/qbsolv_par2_loop_block -p 6500 -g 1700 -i $infile >> $outfile_par2_loop_block";
        echo $par2loopb;
        eval $par2loopb;



    done
done
