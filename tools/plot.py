#!/usr/bin/env python3

"""
Generates a speed-up graph and a summary table of running time for
the PSRS program.
"""

# ------------------------------- MODULE INFO ---------------------------------
__all__ = ["speedup_plot", "runtime_tabulate"]
# ------------------------------- MODULE INFO ---------------------------------

# --------------------------------- MODULES -----------------------------------
import argparse
import math
import matplotlib
import matplotlib.pyplot as plt
import os
import random
import shutil
import struct
import subprocess

from typing import Dict, List, Tuple
# --------------------------------- MODULES -----------------------------------

# ------------------------------ TYPE ALIASES ---------------------------------
# The 'runtime_dict' returned by 'speedup_plot' is a 'dict' with keys
# of the following form:
RunTimeKey = Tuple[int, Tuple[int]]
# an example entry in the 'runtime_dict' would be
# (1024, (1, 2, 4, 8, 16)): [0.5, 0.4, 0.3, 0.2, 0.1]
# which denotes:
# (number of keys sorted, (number of threads used as tests)): [actual runtime]
# ------------------------------ TYPE ALIASES ---------------------------------


# -------------------------------- FUNCTIONS ----------------------------------
def speedup_plot(program: str, output: str) -> Dict[RunTimeKey, List[float]]:
    """
    Plots the speedup graph based on the given 'program' that implements the
    Parallel Sorting by Regular Sampling algorithm and saves the graph as
    'output', also returns a 'dict' containing actual runtimes in the form
    described in 'TYPE ALIASES' section.

    NOTE:
    The PSRS program must support a command line interface of the following:
        ' -b -l {length} -r {run} -s {seed} -t {thread} -w {window}'
    and this function hard-coded the length to be range of:
        [2 ** e for e in range(20, 29, 2)] -> 2 ** 20 -- 2 ** 28 with step 2
    the number of threads is hard-coded to be range of:
        [2 ** e for e in range(5)] -> 2 ** 0 -- 2 ** 4
    the {run} is fixed at 7, and {window} is set to 5.
    """
    if not all(isinstance(check, str) for check in locals().values()):
        raise TypeError("'program' and 'output' must be of 'str' type")

    if not shutil.which(program):
        raise ValueError("'program' is not found")

    average_time = None
    program += " -b -l {length} -r {run} -s {seed} -t {thread} -w {window}"
    argument_dict = dict(run=7, seed=10, window=5)
    thread_range = tuple(2 ** e for e in range(5))
    length_range = tuple(2 ** e for e in range(20, 29, 2))
    # length_range = tuple(2 ** e for e in range(10, 19, 2))
    legend_range = ("o", "s", "^", "d", "*")
    color_range = ("g", "b", "y", "m", "r")
    runtime_keys = [(length, thread_range) for length in length_range]
    runtime_dict = {runtime_key: list() for runtime_key in runtime_keys}
    speedup_vector = list()
    extension = os.path.splitext(output)[-1]

    if not extension:
        raise ValueError("The output must have a valid file extension")

    plt.title("Speedup Graph")
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
        speedup_vector.clear()
        for thread_count in thread_range:
            argument_dict["thread"] = thread_count
            command = program.format(**argument_dict).split(" ")
            # Let 'psrs' program write the moving average in binary form,
            # rather than the human-readable text form, because 'printf' cannot
            # print exact values of floating-point numbers that easily.
            # 'psrs' calls 'fwrite' to write the moving average into the
            # 'subprocess.PIPE', and is parsed by the 'unpack' mechanism.

            # The method 'communicate' returns a tuple of the form
            # (stdout_data, stderr_data)
            # here only the first element is of interest.

            # The result of 'unpack' method call is a tuple regardless of the
            # data to be unpacked; since the output of 'psrs' is of a single
            # double floating-point value, only the first element is needed.
            with subprocess.Popen(command, stdout=subprocess.PIPE) as proc:
                average_time = struct.unpack("@d", proc.communicate()[0])[0]
            if 1 != thread_count:
                # Speedup = T1 / Tp
                speedup = speedup_vector[0] / average_time
            else:
                speedup = average_time
            speedup_vector.append(speedup)
            runtime_dict[(length, thread_range)].append(average_time)

        # The speedup for the 1 thread case is always 1
        # set outside the inner loop because all the speedup values in
        # the 'speedup_vector' need to be calculated based on the T1
        speedup_vector[0] = 1.0
        plt.plot(thread_range, speedup_vector,
                 color=color, label=_log2_exponent_get(length), linestyle="--",
                 marker=legend, markersize=10)

    plt.legend(loc="best", title="Length")
    plt.savefig(output)
    plt.clf()
    return runtime_dict


def runtime_tabulate(runtime: Dict[RunTimeKey, List[float]], output: str):
    """

    NOTE: Assumes all the values in 'runtime' is of same length; so there
    are same number of threads tested for each length.
    """
    if not (isinstance(runtime, dict) and isinstance(output, str)):
        raise TypeError("'runtime' and 'output' need to be of 'dict', 'str'"
                        " types, respectively")

    length_range = [float(key[0]) for key in sorted(runtime.keys())]
    length_label = [_log2_exponent_get(length) for length in length_range]
    thread_range = random.choice(list(runtime.keys()))[-1]
    thread_label = list()
    runtime_matrix = [runtime[key] for key in sorted(runtime.keys())]
    runtime_format = [["{0:f}".format(j) for j in i] for i in runtime_matrix]

    for thread in thread_range:
        label = "{0} Thread{1}".format(thread, "" if 1 == thread else "s")
        thread_label.append(label)

    # plt.axis("tight")
    plt.axis("off")
    plt.title("Running Time in Moving Average (second)")
    table = plt.table(cellText=runtime_format,
                      rowLabels=length_label,
                      colLabels=thread_label,
                      loc="center")
    # table.set_fontsize("large")
    # table.scale(1.2, 1.2)
    table.scale(1, 4.5)
    # figure = plt.gcf()
    # figure.set_size_inches(10, 6)
    plt.savefig(output)
    plt.clf()


def _log2_exponent_get(number: float) -> str:
    """
    Returns a specially formatted string of the result log2(number).

    NOTE: The result log2(number) must be an integer.
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
    attr_desc_dict = {
        "program": "path to the PSRS executable",
        "speedup": "file name of the speed-up program",
        "table": "file name of the running time summary table"
    }

    for flag, msg in attr_desc_dict.items():
        parser.add_argument("-" + flag[0],
                            "--" + flag,
                            type=str,
                            required=False,
                            help=msg)
    args = parser.parse_args()

    if all(getattr(args, attr) for attr in attr_desc_dict):
        matplotlib.rc('font',
                      **{'sans-serif': 'Arial', 'family': 'sans-serif'})
        runtime_dict = speedup_plot(args.program, args.speedup)
        runtime_tabulate(runtime_dict, args.table)
# -------------------------------- FUNCTIONS ----------------------------------


if __name__ == "__main__":
    main()
