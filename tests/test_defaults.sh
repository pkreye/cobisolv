#!/bin/bash
qbsolv="../build-old/qbsolv"

numProbs=10;
probSet="bqp1000";
targetEnergies_bqp1000=(99 371438 354932 371236 370675 352760 359629 371193 351994 349337 351415);

function genOutfileName() {
    echo "./new_p${1}g${2}_time${3}s_${probSet}.data"
}

# echo $(genOutfileName "test");
timings=("0.1" "0.5" "1" "5" "10" "30" "60")

# echo "Timings: " ${timings[@]}

outfiles=();
for t in "${timings[@]}"
do
    outfiles+=($(genOutfileName 0 1000 $t));
done


# qbsolv parameters
qb_p=0;
qb_g=1000;

filePrefix="p${qb_p}g${qb_g}_target";
# outfile="./${filePrefix}_seq_${probSet}.data"

numRuns=10
outfileCount=0
for i in $(seq 1 $numRuns); do
    randNum1=$(eval "od -A n -t d -N 3 /dev/urandom")
    # randNum2=$(eval "od -A n -t d -N 3 /dev/urandom")
    for probNum in $(eval echo "{1..$numProbs}");
    do
        infile="./qubos/${probSet}_${probNum}.qubo";

        commonArgs="-m -p $qb_p -g $qb_g -T ${targetEnergies_bqp1000[$probNum]} -i $infile"

        # seq="../build/qbsolv_seq $commonArgs >> $outfile_seq";
        # echo $seq;
        # eval $seq;
        outfileCount=0
        for t in "${timings[@]}"
        do
            echo "$qbsolv -t $t -r $randNum1 $commonArgs >> ${outfiles[$outfileCount]}";

            $qbsolv -t $t -r $randNum1 $commonArgs >> "${outfiles[$outfileCount]}"
            # $qbsolv -r $randNum2 $commonArgs >> "$outfile" &
            # wait

            outfileCount=$(($outfileCount + 1))
        done
    done
done

# cmd="cat ${outfile} | sed 's/^.*_//' | sed 's/\.qubo//' | sed 's/[[:space:]]\+/ /g' > ${outfile}.probnum"
# echo $cmd
# eval $cmd

for file in "${outfiles[@]}"
do
    cat "${file}.p1" >> "$file";
    cat "${file}.p2" >> "$file";

    cmd="cat ${file} | sed 's/^.*_//' | sed 's/\.qubo//' | sed 's/[[:space:]]\+/ /g' > ${file}.probnum"
    echo $cmd
    eval $cmd
done
