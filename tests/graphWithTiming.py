import matplotlib as mpl
import matplotlib.pyplot as plt
import numpy as np

import seaborn as sns

import csv
import sys

filePrefix = "foo"
files = {
    "0.1" : "foo_time0.1s_seq_bqp1000.data.probnum",
    "0.5" : "foo_time0.5s_seq_bqp1000.data.probnum",
    "1"   : "foo_time1s_seq_bqp1000.data.probnum",
    "2"   : "foo_time2s_seq_bqp1000.data.probnum",
    "5"   : "foo_time5s_seq_bqp1000.data.probnum",
    "10"  : "foo_time10s_seq_bqp1000.data.probnum",
    "15"  : "foo_time15s_seq_bqp1000.data.probnum",
    "30"  : "foo_time30s_seq_bqp1000.data.probnum",
    "60"  : "foo_time60s_seq_bqp1000.data.probnum"
}

TARGET_ENERGIES = [371438, 354932, 371236, 370675, 352760, 359629, 371193, 351994, 349337, 351415]

def read_data_file(fileName):
    data = []
    with open(fileName, newline='') as f:
        reader = csv.reader(f, delimiter=' ')
        for row in reader:
            try:
                probNum = int(row[0])
                energy = int(float(row[3]))
                time = float(row[8])

                data.append([probNum, energy, time])
            except ValueError as e:
                sys.exit('Error parsing: {} {}'.format(row[3], row))

    return data


def build_cumulative(data):
    cdata = {}
    return cdata

def graph_cumulative(data):
    return

def process_data(data, solvedCondition):
    tts = {}
    runTimes = {}
    solvedCount = {}
    solvedCondCount = {}
    numRuns = 0

    for row in data:
        numRuns += 1
        probNum = row[0]
        energy = row[1]
        finalTime = row[2]

        # initialize result tables

        if probNum not in runTimes:
            runTimes[probNum] = []

        if probNum not in solvedCount:
            solvedCount[probNum] = 0

        if probNum not in solvedCondCount:
            solvedCondCount[probNum] = 0

        # Add data to result tables

        runTimes[probNum].append(finalTime)

        if energy == TARGET_ENERGIES[probNum - 1]:
            solvedCount[probNum] += 1

            if probNum not in tts:
                tts[probNum] = []

            tts[probNum].append(finalTime)

        if energy >= solvedCondition * TARGET_ENERGIES[probNum - 1]:
            solvedCondCount[probNum] += 1

    return tts, solvedCount, solvedCondCount, numRuns, runTimes

# def num_probs_solved(energy)

if __name__ == "__main__":
    colors = sns.color_palette()

    solvedCond = 0.9999
    xdata = []
    ydata = []
    palette = []

    solvedProbs = {}

    for k in files:
        fname = files[k]
        print("File: {}".format(fname))

        data = read_data_file(fname)
        tts, solvedCount, solvedCondCount, numRuns, runTimes = process_data(data, solvedCond)

        print("Problems #'s solved: {}".format(list(tts.keys())))

        # calculte aggregate stats

        runTimeAvgs = {}
        for probNum in runTimes:
            runTimeAvgs[probNum] = sum(runTimes[probNum]) / len(runTimes[probNum])

        totalSolved = 0
        totalCondSolved = 0
        for probNum in solvedCount:
            totalSolved += solvedCount[probNum]
            totalCondSolved += solvedCondCount[probNum]

            print("problem {}, average run time {}".format(
                probNum,
                runTimeAvgs[probNum]
            ))

        # xdata.append(k)
        # ydata.append(totalSolved)
        # palette.append(colors[0])

        xdata.append(float(k))
        ydata.append(totalCondSolved / numRuns)
        palette.append(colors[0])


    sns.barplot(x=xdata, y=ydata, palette=palette)
    plt.title("")
    plt.xlabel("Time Limit (seconds)")
    plt.ylabel("Fraction solved (condition: {})".format(solvedCond))
    plt.show()

        # cData = build_cumulative(d)
        # graph_cumulative(cData)

    # plt.plot([1, 2, 3, 4])
    # plt.ylabel('some numbers')
    # plt.show()
