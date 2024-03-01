#!/bin/bash
qbsolv="./qbsolv"

numProbs=10;
#probSet="bqp1000";
#targetEnergies_bqp1000=(99 371438 354932 371236 370675 352760 359629 371193 351994 349337 351415);

function genOutfileName() {
    echo "./${1}_p${2}g${3}_${4}.data"
}

function getSubQuboSolverName() {
    if [ "${1}" = "-I" ];
    then
        echo "ising100u";
    elif [ "${1}" = "-I -d 150" ];
    then
        echo "ising150u";
    elif [ "${1}" = "-I -d 200" ];
    then
        echo "ising200u"
    elif [ "${1}" = "-I -d 250" ];
    then
        echo "ising250u"
    elif [ "${1}" = "-R" ];
    then
         echo "rand";
    elif [ "${1}" = "" ];
    then
        echo "tabu";
    else
        echo "unknown";
    fi
}

# echo $(genOutfileName "test");
# timings=("0.1" "0.5" "1" "5" "10" "30" "60")

# echo "Timings: " ${timings[@]}

outfiles=();
# for t in "${timings[@]}"
# do
#
# done


# qbsolv parameters
qb_p=0;
qb_g=0;


num_incrs=2
g_incr=100;

filePrefix="p${qb_p}g${qb_g}_target";
# outfile="./${filePrefix}_seq_${probSet}.data"
# "-I -d 150" "-I -d 250"
subQuboOptions=("-I" "-I -d 200" "-R" "");
probSets=("bqp50" "bqp100"); #"bqp250" "bqp500"
numRuns=5
outfileCount=0
for j in $(seq 0 2);
do
    for i in $(seq 1 $numRuns); do
        randNum1=$(eval "od -A n -t d -N 3 /dev/urandom")
        # randNum2=$(eval "od -A n -t d -N 3 /dev/urandom")
        for probSet in "${probSets[@]}";
        do
            for subQuboOption in "${subQuboOptions[@]}";
            do
                subQuboSolver=$(getSubQuboSolverName "$subQuboOption");

                outfile=$(genOutfileName $probSet "$qb_p" "$qb_g" "$subQuboSolver");
                outfiles+=$outfile;
                for probNum in $(eval echo "{1..$numProbs}");
                do
                    infile="../qubos/${probSet}_${probNum}.qubo";

                    commonArgs="$subQuboOption -r $randNum1 -m -p $qb_p -g $qb_g -i $infile"

                    echo "$qbsolv $commonArgs >> $outfile";

                    # seq="../build/qbsolv_seq $commonArgs >> $outfile_seq";
                    # echo $seq;
                    # eval $seq;
                    # outfileCount=0
                    # for t in "${timings[@]}"
                    # do


                    # $qbsolv -t $t -r $randNum1 $commonArgs >> "${outfiles[$outfileCount]}"
                    # $qbsolv -r $randNum2 $commonArgs >> "$outfile" &
                    # wait

                    # outfileCount=$(($outfileCount + 1))
                    # done
                done
            done
        done
    done
    qb_g=$(($qb_g + $g_incr));
done
# cmd="cat ${outfile} | sed 's/^.*_//' | sed 's/\.qubo//' | sed 's/[[:space:]]\+/ /g' > ${outfile}.probnum"
# echo $cmd
# eval $cmd

# for file in "${outfiles[@]}"
# do
#     # cat "${file}.p1" >> "$file";
#     # cat "${file}.p2" >> "$file";

#     cmd="cat ${file} | sed 's/^.*_//' | sed 's/\.qubo//' | sed 's/[[:space:]]\+/ /g' > ${file}.probnum"
#     echo $cmd
#     eval $cmd
# done
