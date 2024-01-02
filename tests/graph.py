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

def process_data(data, globalSearchFactorFilter, solvedCondition=1):
    tts = {}
    runTimes = {}
    solvedCount = {}

    numRuns = 0

    prevProbNum = 1

    probTimes = {}
    probSetTimes = []
    curProbTime = 0

    curProbSolved = False
    for row in data:
        numRuns += 1

        probNum = row[0]
        preSearchFactor = row[1]
        globalSearchFactor = row[2]
        energy = row[3]
        finalTime = row[4]

        #print("row {}".format(row))
        if preSearchFactor != 0 or globalSearchFactor != globalSearchFactorFilter:
            # filter data based on search params
            #print("Continue: filter")
            continue

        if probNum != prevProbNum:
            # starting runs of new problem
            curProbSolved = False
            if probNum == 1 and prevProbNum > 1:
                # we've finished the set of problems and are starting the next iteration of the set
                try:
                    probSetTimes.append(sum([probTimes[k][-1] for k in probTimes]))
                except:
                    print("Incomplete solution set for condition {}.\n probTimes: {}".format(solvedCondition, probTimes))

        if curProbSolved:
            #print("Continue: solution condition")
            continue

        # initialize result tables

        if probNum not in probTimes:
            probTimes[probNum] = []

        if probNum not in runTimes:
            runTimes[probNum] = []

        if probNum not in solvedCount:
            solvedCount[probNum] = 0

        # Track time to solution, cumulative and individual

        if probNum == prevProbNum:
            curProbTime += finalTime
        else:
            # moving on to next problem, reinitialize curProbTime
            curProbTime = finalTime

        # Add data to result tables
        runTimes[probNum].append(finalTime)

        if energy >= solvedCondition * TARGET_ENERGIES[probNum - 1]:
            solvedCount[probNum] += 1
            curProbSolved = True
            probTimes[prevProbNum].append(curProbTime)
            if probNum not in tts:
                tts[probNum] = []
            tts[probNum].append(finalTime)

        prevProbNum = probNum
    # TODO Clean up this whole mess..
    # for now.. don't forget the final problem..
    probTimes[prevProbNum].append(curProbTime)
    curProbTime = finalTime
    probSetTimes.append(sum([probTimes[k][-1] for k in probTimes]))

    return tts, solvedCount, runTimes, probTimes, probSetTimes

def graph_cumulativeAvgTime(probTimes, globalSearchParam, filePrefix, problemSetName, outdir, show=False, save=False):
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

    if solvedCond != 1:
        plt.title("{} Avg (Global search factor: {}, Solved Condition: {})".format(problemSet, globalSearchParam, solvedCond))
    else:
        plt.title("{} Avg (global search factor: {})".format(problemSet, globalSearchParam))

    plt.xlabel("Total time (seconds)")
    plt.ylabel("Fraction Solved")

    if show:
        plt.show()

    if save:
        if solvedCond != 1:
            outfile = "{}/{}_{}_cumulative_avg_p0g{}_cond{}.png".format(outdir, filePrefix, problemSet, globalSearchParam, solvedCond)
        else:
            outfile = "{}/{}_{}_cumulative_avg_p0g{}.png".format(outdir, filePrefix, problemSet, globalSearchParam)
        print("Saving figure to {}".format(outfile))
        plt.savefig(outfile)
    plt.clf()
    return

def graph_cumulativeProblemSetTime(probTimes, globalSearchParam, filePrefix, problemSetName, outdir, show=False, save=False):
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

        if solvedCond != 1:
            plt.title("{} Run {} (Global search factor: {}, Solved Condition: {})".format(probSetCount, problemSet, globalSearchParam, solvedCond))
        else:
            plt.title("{} Run {} (global search factor: {})".format(probSetCount, problemSet, globalSearchParam))

        plt.xlabel("Total time (seconds)")
        plt.ylabel("Fraction Solved")

        if show:
            plt.show()

        if save:
            if solvedCond != 1:
                outfile = "{}/{}_{}_cumulative_p0g{}_cond{}_{}.png".format(outdir, filePrefix, problemSet, globalSearchParam, solvedCond, probSetCount)
            else:
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

    solvedCond = 0.99

    data = read_data_file(inputFile)

    for globalSearchParam in [50, 100, 1000]:
        print("Global search param: {}".format(globalSearchParam))
        tts, solvedCount, runTimes, probTimes, probSetTimes = process_data(data, globalSearchParam)

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
        for probNum in solvedCount:
            totalSolved += solvedCount[probNum]

            print("problem {}, solved condition {}, average run time {}".format(
                probNum,
                solvedCond,
                runTimeAvgs[probNum]
            ))


        graph_cumulativeAvgTime(probTimes, problemSet, filePrefix, globalSearchParam, outdir, show=True, save=False)
        graph_cumulativeProblemSetTime(probTimes, problemSet, filePrefix, globalSearchParam, outdir, show=True, save=False)
