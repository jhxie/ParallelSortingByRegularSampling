#!/usr/bin/env python3

"""
Generates speed-up graphs for the PSRS program.
"""

# ------------------------------- MODULE INFO ---------------------------------
__all__ = ["speedup_plot"]
# ------------------------------- MODULE INFO ---------------------------------

# --------------------------------- MODULES -----------------------------------
import argparse
import math
import matplotlib
import matplotlib.pyplot as plt
import os
# --------------------------------- MODULES -----------------------------------


# -------------------------------- FUNCTIONS ----------------------------------
def speedup_plot(program: str, output: str):
    """
    Plots the speedup graph based on the given 'program' that implements the
    Parallel Sorting by Regular Sampling algorithm and saves the graph as
    'output'.

    NOTE:
    The PSRS program must have a command line interface of the following:
        ' -l {length} -r {run} -s {seed} -t {thread}'
    and this function hard-coded the length to be range of:
        [2 ** e for e in range(20, 29, 2)] -> 2 ** 20 -- 2 ** 28 with step 2
    the number of threads is hard-coded to be range of:
        [2 ** e for e in range(5)] -> 2 ** 0 -- 2 ** 4
    the {run} is fixed at 5.
    """
    if not all(isinstance(check, str) for check in locals().values()):
        raise TypeError("'program' and 'output' must be of 'str' type")

    average_time = None
    program += " -l {length} -r {run} -s {seed} -t {thread}"
    argument_dict = dict(run=5, seed=10)
    thread_range = [2 ** e for e in range(5)]
    length_range = [2 ** e for e in range(20, 29, 2)]
    #  length_range = [2 ** e for e in range(15, 24, 2)]
    legend_range = ["o", "s", "^", "d", "*"]
    color_range = ["g", "b", "y", "m", "r"]
    speedup = list()
    extension = os.path.splitext(output)[-1]

    if not extension:
        raise ValueError("The output must have a valid file extension")

    plt.xticks(thread_range)
    plt.yticks(thread_range)
    plt.xlabel("Number of Threads", fontsize="large")
    plt.ylabel(r"Speedup ($T_1$ / $T_p$)", fontsize="large")
    # The format for axis range is [xmin, xmax, ymin, ymax].
    plt.axis([0, thread_range[-1] + 2, 0, thread_range[-1] + 2])
    # The Linear Speedup Reference Line
    plt.plot(thread_range, thread_range,
             color="c", label="Linear", linestyle="--",
             marker="+", markersize=10)

    for length, legend, color in zip(length_range, legend_range, color_range):
        argument_dict["length"] = length
        speedup.clear()
        for thread_count in thread_range:
            argument_dict["thread"] = thread_count
            with os.popen(program.format(**argument_dict)) as proc:
                average_time = float(proc.read().strip())
            if 1 != thread_count:
                # Speedup = T1 / Tn
                average_time = speedup[0] / average_time
            speedup.append(average_time)

        # The speedup for the 1 thread case is always 1
        speedup[0] /= speedup[0]
        plt.plot(thread_range, speedup,
                 color=color, label=_log2_exponent_get(length), linestyle="--",
                 marker=legend, markersize=10)

    plt.legend(loc="best", title="Length")
    plt.savefig(output)


def _log2_exponent_get(number: float) -> str:
    """
    """
    result = math.log2(number)

    if not result.is_integer():
        raise ValueError("The result exponent must be an integer")

    result = int(result)
    return r"$\mathregular{2^{" + str(result) + r"}}$"


def main():
    """
    Command line driver for the 'speedup_plot' function.
    """

    parser = argparse.ArgumentParser()
    parser.add_argument("-p",
                        "--program",
                        type=str,
                        required=False,
                        help="path to the PSRS executable")
    parser.add_argument("-o",
                        "--output",
                        type=str,
                        required=False,
                        help="file name of the speed-up program")
    args = parser.parse_args()

    if args.program and args.output:
        matplotlib.rc('font',
                      **{'sans-serif': 'Arial', 'family': 'sans-serif'})
        speedup_plot(args.program, args.output)
# -------------------------------- FUNCTIONS ----------------------------------


if __name__ == "__main__":
    main()
