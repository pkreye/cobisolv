import matplotlib as mpl
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import seaborn as sns

import csv
import sys

TARGET_ENERGIES_BQP50   = [2098, 3702, 4626, 3544, 4012, 3693, 4520, 4216, 3780, 3507]
TARGET_ENERGIES_BQP100  = [7970, 11036, 12723, 10368, 9083, 10210, 10125, 11435, 11455, 12565]
TARGET_ENERGIES_BQP250  = [45607, 44810, 49037, 41274, 47961, 41014, 46757, 35726, 48916, 40442]
TARGET_ENERGIES_BQP500  = [116586, 128339, 130812, 130097, 125487, 121772, 122201, 123559, 120798, 130619]
TARGET_ENERGIES_BQP1000 = [371438, 354932, 371236, 370675, 352760, 359629, 371193, 351994, 349337, 351415]
TARGET_ENERGIES_BQP2500 = [1515944, 1471392, 1414192, 1507701, 1491816, 1469162, 1479040, 1484199, 1482413, 1483355]

TARGET_ENERGIES = {
    "bqp50" : TARGET_ENERGIES_BQP50,
    "bqp100" : TARGET_ENERGIES_BQP100,
    "bqp250" : TARGET_ENERGIES_BQP250,
    "bqp500" : TARGET_ENERGIES_BQP500,
    "bqp1000" : TARGET_ENERGIES_BQP1000,
    "bqp2500" : TARGET_ENERGIES_BQP2500,
}


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

def process_data_for_runs(data, probSet, globalSearchFactorFilter, solvedCondition=1):
    # TODO fix this
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

        if energy >= solvedCondition * TARGET_ENERGIES[probSet][probNum - 1]:
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

def process_data(data, probSet, globalSearchFactorFilter, solvedCondition=1):
    tts = {}
    runTimes = {}
    solvedCount = {}

    energies = {}

    numRuns = 0

    # probTimes = {}
    # probSetTimes = []
    # curProbTime = 0

    # curProbSolved = False
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

        # initialize result tables

        if probNum not in energies:
            energies[probNum] = []

        # if probNum not in probTimes:
        #     probTimes[probNum] = []

        # if probNum not in runTimes:
        #     runTimes[probNum] = []

        if probNum not in solvedCount:
            solvedCount[probNum] = 0

        # Track time to solution, cumulative and individual

        # if probNum == prevProbNum:
        #     curProbTime += finalTime
        # else:
            # moving on to next problem, reinitialize curProbTime
            # curProbTime = finalTime

        # Add data to result tables
        # runTimes[probNum].append(finalTime)

        energies[probNum].append(energy)

        if energy >= solvedCondition * TARGET_ENERGIES[probSet][probNum - 1]:
            solvedCount[probNum] += 1
            # probTimes[probNum].append(curProbTime)

    # TODO Clean up this whole mess..
    # for now.. don't forget the final problem..

    return energies, solvedCount

def graph_cumulativeAvgTime(probTimes, globalSearchParam, filePrefix, problemSetName, outdir, show=False, save=False, clear=False):
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

    if clear:
        plt.clf()
    return

def graph_cumulativeProblemSetTime(probTimes, globalSearchParam, filePrefix, problemSetName, outdir):
    pivotTimes = []

    numSets = None
    probSetCount = 1

    result = ([],[])

    for probNum in probTimes:
        lenTest = len(probTimes[probNum])
        if numSets is None:
            numSets = lenTest
        elif lenTest != numSets:
            print("Error: Inconsistent number of solutions: problem set number {}: len {}".format(probSetCount, lenTest))
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

        result[0].append(xdata)
        result[1].append(ydata)

        # p = sns.lineplot(x=xdata, y=ydata, marker='o')

        # if solvedCond != 1:
        #     plt.title("{} Run {} (Global search factor: {}, Solved Condition: {})".format(probSetCount, problemSet, globalSearchParam, solvedCond))
        # else:
        #     plt.title("{} Run {} (global search factor: {})".format(probSetCount, problemSet, globalSearchParam))

        # plt.xlabel("Total time (seconds)")
        # plt.ylabel("Fraction Solved")

        # if show:
        #     plt.show()

        # if save:
        #     if solvedCond != 1:
        #         outfile = "{}/{}_{}_cumulative_p0g{}_cond{}_{}.png".format(outdir, filePrefix, problemSet, globalSearchParam, solvedCond, probSetCount)
        #     else:
        #         outfile = "{}/{}_{}_cumulative_p0g{}_{}.png".format(outdir, filePrefix, problemSet, globalSearchParam, probSetCount)
        #     print("Saving figure to {}".format(outfile))
        #     plt.savefig(outfile)
        # if clear:
        #     plt.clf()

        probSetCount += 1
    return result

if __name__ == "__main__":
    outdir = "/home/foobar/Pictures/qbsolv/ising/"
    problemSets = ["bqp100", "bqp250", "bqp1000", "bqp2500"]
    globalSearchFactors = [0, 100] #, 100, 200
    subProbSolvers = ["ising100u", "ising200u", "tabu"]
    inputPrefix = "./ising_data_0/"
    # input file format: <probset>_p0g<>_<solver>.data
    # inputFiles = [

    #     for probSet in problemSets
    #     for g in globalSearchFactors
    #     for solver in subProbSolvers
    # ]

    colors = sns.color_palette()

    solvedCond = 1

    tmp_data = []
    fracs = []
    xdata = []
    ydata = []
    for probSet in problemSets:
        problemData = []
        for globalSearchParam in globalSearchFactors:
            for solver in subProbSolvers:
                inputFile = "{}{}_p0g{}_{}.data.probnum".format(
                    inputPrefix, probSet, globalSearchParam, solver
                )
                data = read_data_file(inputFile)

                energies, solvedCount = process_data(data, probSet, globalSearchParam)

                # print(energies)
                # print(solvedCount)
                # print("Problem Times:\n")
                # for k in probTimes:
                #     print("problem {}, length {}, {}".format(k, len(probTimes[k]), probTimes[k]))

                # print("Problem Set Times:\n{}".format(probSetTimes))

                # print("Problems #'s solved: {}".format(list(tts.keys())))

                # calculte aggregate stats

                # palette = []
                solvedProbs = {}

                # runTimeAvgs = {}
                # for probNum in runTimes:
                #     runTimeAvgs[probNum] = sum(runTimes[probNum]) / len(runTimes[probNum])
                print("\n--Solver: {}, prob set: {}, global search factor: {}".format(
                    solver, probSet, globalSearchParam
                ))

                for probNum in energies:
                    target = TARGET_ENERGIES[probSet][probNum - 1]
                    problemData.extend(list(map(lambda x: ["{}, {}".format(solver, globalSearchParam), round(x / target,4)], energies[probNum])))
                    xdata.extend([probNum] * len(energies[probNum]))
                    #print("problem {}: {}".format(probNum, ", ".join(ydata)))

                # print("--")
                # print(problemData)
                # print("--")
        xlabel = "Test params"
        ylabel = "energies"
        df = pd.DataFrame(problemData, columns=[xlabel, ylabel])
                #print(df)
        p = sns.boxplot(df, y=ylabel, x=xlabel)
        # plt.xticks(rotation="vertical")
        plt.title("Problem set: {}".format(probSet))
        plt.xticks(rotation=30, ha='right')

        outputFile = "{}{}.png".format(
            outdir, probSet
        )

        plt.savefig(outputFile, dpi=300, bbox_inches="tight")
        plt.show()



            # totalSolved = 0
            # for probNum in solvedCount:
            #     totalSolved += solvedCount[probNum]

            #     print("problem {}, solved condition {}, average run time {}".format(
            #         probNum,
            #         solvedCond,
            #         runTimeAvgs[probNum]
            #     ))


        # #graph_cumulativeAvgTime(probTimes, problemSet, filePrefix, globalSearchParam, outdir, show=True, save=False, clear=False)
        # tmp_data.append(graph_cumulativeProblemSetTime(probTimes, globalSearchParam, filePrefix, problemSet, outdir))
        # print(len(tmp_data))
        # xdata = sorted(set(tmp_data[0][1] + tmp_data[1][1] + tmp_data[2][1]))

        # df = pd.DataFrame({'Time (seconds)': xdata, 'Global search factor: 50': tmp_data[0][2], 'Global search factor: 100': tmp_data[1][2], 'Global search factor: 1000': tmp_data[2][2]})
        # sns.lineplot(df)
    #plt.show()
    #plt.clf()
