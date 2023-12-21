import matplotlib as mpl
import matplotlib.pyplot as plt
import numpy as np

import seaborn as sns

import csv
import sys

TARGET_ENERGIES = [371438, 354932, 371236, 370675, 352760, 359629, 371193, 351994, 349337, 351415]

def read_data_file(fileName):
    data = []
    with open(fileName, newline='') as f:
        reader = csv.reader(f, delimiter=' ')
        for row in reader:
            try:
                probNum = int(row[0])
                preSearchFactor = int(row[1])
                globalSearchFactor = int(row[2])
                energy = int(float(row[3]))
                time = float(row[8])
                # seed = int(row[9])
                # timeLimit = float(row[10])

                data.append([probNum, preSearchFactor, globalSearchFactor, energy, time])
            except ValueError as e:
                sys.exit('Error parsing: {} {}'.format(row[3], row))

    return data

# def build_cumulative(data):
#     cdata = {}
#     return cdata

# def graph_cumulative(data):
#     return

def process_data(data, solvedCondition, globalSearchFactorFilter, useCondition=False):
    tts = {}
    runTimes = {}
    solvedCount = {}
    solvedCondCount = {}
    numRuns = 0

    prevProbNum = 1

    probTimes = {}
    probSetTimes = []
    curProbTime = 0

    for row in data:
        numRuns += 1

        probNum = row[0]
        preSearchFactor = row[1]
        globalSearchFactor = row[2]
        energy = row[3]
        finalTime = row[4]

        if preSearchFactor != 0 or globalSearchFactor != globalSearchFactorFilter:
            # filter data based on search params
            continue

        # initialize result tables

        if probNum not in probTimes:
            probTimes[probNum] = []

        if probNum not in runTimes:
            runTimes[probNum] = []

        if probNum not in solvedCount:
            solvedCount[probNum] = 0

        if probNum not in solvedCondCount:
            solvedCondCount[probNum] = 0

        #

        if probNum == prevProbNum:
            curProbTime += finalTime
        else:
            probTimes[prevProbNum].append(curProbTime)
            curProbTime = finalTime
            if probNum == 1 and prevProbNum > 1:
                # we've finished the problem set and are starting the next run
                probSetTimes.append(sum([probTimes[k][-1] for k in probTimes]))

        # Add data to result tables

        runTimes[probNum].append(finalTime)

        if energy == TARGET_ENERGIES[probNum - 1]:
            solvedCount[probNum] += 1
            if probNum not in tts:
                tts[probNum] = []
            tts[probNum].append(finalTime)

        if energy >= solvedCondition * TARGET_ENERGIES[probNum - 1]:
            solvedCondCount[probNum] += 1
        prevProbNum = probNum
    # TODO Clean up this whole mess..
    # for now.. don't forget the final problem..
    probTimes[prevProbNum].append(curProbTime)
    curProbTime = finalTime
    if probNum == 1 and prevProbNum > 1:
        # we've finished the problem set and are starting the next run
        probSetTimes.append(sum([probTimes[k][-1] for k in probTimes]))

    return tts, solvedCount, solvedCondCount, runTimes, probTimes, probSetTimes

def graph_cumulativeAvgTime(probTimes, problemSetName, filePrefix, globalSearchParam, outdir, show=False):
    xdata = []
    ydata = []
    cumulativeTime = 0
    numProbs = len(probTimes.keys())
    count = 1
    for probNum in probTimes:
        sumTime = 0
        for runTime in probTimes[probNum]:
            sumTime += runTime
        avgTime = sumTime / len(probTimes[probNum])
        cumulativeTime += avgTime
        xdata.append(cumulativeTime)
        ydata.append(count / numProbs)
        count += 1

    p = sns.lineplot(x=xdata, y=ydata)
    plt.title("{} (global search factor: {})".format(problemSet, globalSearchParam))
    plt.xlabel("Total time (seconds)")
    plt.ylabel("Fraction Solved")

    if show:
        plt.show()

    outfile = "{}/{}_{}_p0g{}.png".format(outdir, filePrefix, problemSet, globalSearchParam)
    print("Saving figure to {}".format(outfile))
    plt.savefig(outfile)
    return

def graph_cumulativeProblemSetTime(probTimes, problemSetName, filePrefix, globalSearchParam, outdir, show=False):
    pivotTimes = []

    numSets = None
    probSetCount = 1

    for probNum in probTimes:
        lenTest = len(probTimes[probNum])
        if numSets is None:
            numSets = lenTest
        elif lenTest != numSets:
            print("Error: Inconsistent number of solutions: problem set number {}".format(probSetCount))
        probSetCount += 1

    for setNum in range(numSets):
        pivotTimes.append([])
        for probNum in probTimes:
            pivotTimes[-1].append(probTimes[probNum][setNum])

    # print("Pivot test:")
    # for r in pivotTimes:
    #     print("{}".format(r))
    probSetCount = 1
    for probSet in pivotTimes:
        xdata = []
        ydata = []
        cumulativeTime = 0
        solvedCount = 1
        for runTime in probSet:
            cumulativeTime += runTime
            xdata.append(cumulativeTime)
            ydata.append(solvedCount / len(probSet))
            solvedCount += 1

        print("xdata {}, \nydata {}".format(xdata, ydata))
        p = sns.lineplot(x=xdata, y=ydata)
        plt.title("{} (global search factor: {}, run: {})".format(problemSet, globalSearchParam, probSetCount))
        plt.xlabel("Total time (seconds)")
        plt.ylabel("Fraction Solved")

        if show:
            plt.show()

        outfile = "{}/{}_{}_cumulative_p0g{}_{}.png".format(outdir, filePrefix, problemSet, globalSearchParam, probSetCount)
        print("Saving figure to {}".format(outfile))
        plt.savefig(outfile)
        plt.clf()

        probSetCount += 1


    return

if __name__ == "__main__":
    outdir = "/home/panta/Pictures/qbsolv"
    problemSet = "bqp1000"
    filePrefix = "tts"
    inputFile = "{}_{}.data.probnum".format(filePrefix, problemSet)

    colors = sns.color_palette()

    solvedCond = 0.9999

    data = read_data_file(inputFile)

    for globalSearchParam in [50, 100, 1000]:
        print("Global search param: {}".format(globalSearchParam))
        tts, solvedCount, solvedCondCount, runTimes, probTimes, probSetTimes = process_data(data, solvedCond, globalSearchParam)

        print("Problem Times:\n")
        for k in probTimes:
            print("problem {}, length {}, {}".format(k, len(probTimes[k]), probTimes[k]))

        print("Problem Set Times:\n{}".format(probSetTimes))

        print("Problems #'s solved: {}".format(list(tts.keys())))

        # calculte aggregate stats

        # palette = []
        solvedProbs = {}

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


        # xdata, ydata = cumulativeAvgTime(probTimes)
        graph_cumulativeProblemSetTime(probTimes, problemSet, filePrefix, globalSearchParam, outdir, show=False)
