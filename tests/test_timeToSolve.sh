#!/bin/bash

qbsolv="../build-old/qbsolv"

numProbs=10;
probName="bqp1000";
targetEnergies_bqp1000=(0 371438 354932 371236 370675 352760 359629 371193 351994 349337 351415);
targetEnergies_bqp1000_99=(0 367723 351382 367523 366968 349232 356032 367481 348474 345843 347900);
targetEnergies_bqp100_999=(371067, 354578, 370865, 370305, 352408, 359270, 370822, 351643, 348988, 351064)
# targetEnergies_bqp100=(99 7970 11036 12723 10368 9083 10210 10125 11435 11455 12565);

filePrefix="new_tts_target999";
outfile="${filePrefix}_${probName}.data"
successOutfile="${filePrefix}_success_${probName}.data"

# timings=("0.1" "0.5" "1" "1.5" "2"  "5" "10" "15" "30" "60");
# timeLimit="0.5";

numRuns=1;
numAttempts=10
probNum=1;

# qbsolv parameters
g_params=(1000 100 50 1 0)
qb_p=0;
# qb_g=1;
for qb_g in "${g_params[@]}";
do
    for r in $(seq 1 $numRuns); do
        for probNum in $(eval echo "{1..$numProbs}");
        do
            infile="./qubos/${probName}_${probNum}.qubo";
            probSolved="false";
            timeLimit="1"
            while [[ "$probSolved" == "false" ]];
            do
                for attemptNum in $(seq 1 $numAttempts); do
                    seed=$(od -A n -t d -N 3 /dev/urandom | tr -d " ");

                    echo "$qbsolv -m -t $timeLimit -r $seed -p $qb_p -g $qb_g -i $infile";

                    data=$($qbsolv -m -n 10 -t $timeLimit -r $seed -p $qb_p -g $qb_g -i $infile)
                    echo "$data $timeLimit" >> $outfile;

                    energy=$(printf "%.0f" $(echo "$data" | awk '{print $4}'));

                    if [[ $energy -gt ${targetEnergies_bqp1000_999[$probNum]} ]]; then
                        probSolved="true";
                        echo "$data $timeLimit" >> $successOutfile;
                        echo "Success!";
                        break
                    fi
                done

                timeLimit=$(echo "$timeLimit + 0.5" | bc -l);
            done
            echo "done";
        done
    done
done
