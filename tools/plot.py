#!/usr/bin/env python3

"""
Generates a series of figures, tables, and plots for the PSRS program.
"""

# ------------------------------- MODULE INFO ---------------------------------
__all__ = ["speedup_plot", "runtime_tabulate", "runtime_plot",
           "stdev_tabulate", "phase_pie_plot"]
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

# Note 'Axes3d' is used implicitly by matplotlib
from mpl_toolkits.mplot3d import Axes3D
from typing import Dict, List, Tuple
# --------------------------------- MODULES -----------------------------------

# ------------------------------ TYPE ALIASES ---------------------------------
# The 'runtime_dict' returned by 'speedup_plot' is a 'dict' with keys
# of the following form:
RunTimeKey = Tuple[int, Tuple[int]]
ValPair = Tuple[float, float]
# an example entry in the 'runtime_dict' would be
# (1024, (1, 2, 4, 8)): [[0.4, 0.1], [0.3, 0.1], [0.2, 0.1], [0.1 0.1]]
# which denotes:
# (number of keys sorted, (number of processes used as tests)):
# [[sorting time, standard deviation (error)]]
# ------------------------------ TYPE ALIASES ---------------------------------


# -------------------------------- FUNCTIONS ----------------------------------
def speedup_plot(program: str, output: str) -> Dict[RunTimeKey, List[ValPair]]:
    """
    Plots the speedup graph based on the given 'program' that implements the
    Parallel Sorting by Regular Sampling algorithm and saves the graph as
    'output', also returns a 'dict' containing actual runtimes in the form
    described in 'TYPE ALIASES' section.

    NOTE:
    The PSRS program must support a command line interface of the following:
        ' -b -l {length} -r {run} -s {seed} -w {window}'
    and this function hard-coded the length to be range of:
        [2 ** e for e in range(21, 28, 2)] -> 2 ** 21 -- 2 ** 27 with step 2
    the number of processes is hard-coded to be range of:
        [2 ** e for e in range(4)] -> 2 ** 0 -- 2 ** 3
    the {run} is fixed at 7, and {window} is set to 5.

    Reference:
    https://docs.python.org/3/library/subprocess.html
    """
    if not all(isinstance(check, str) for check in locals().values()):
        raise TypeError("'program' and 'output' must be of 'str' type")

    if not shutil.which(program):
        raise ValueError("'program' is not found")

    if not shutil.which("mpiexec"):
        raise ValueError("'mpiexec' is not found")

    mean_time = None
    std_err = None
    mpi_prefix = "mpiexec -n {process} "
    psrs_flags = " -b -l {length} -r {run} -s {seed} -w {window}"
    program = mpi_prefix + program + psrs_flags
    argument_dict = dict(run=7, seed=10, window=5)
    process_range = tuple(2 ** e for e in range(4))
    # length_range = tuple(2 ** e for e in range(21, 28, 2))
    # length_range = tuple(2 ** e for e in range(19, 26, 2))
    length_range = tuple(2 ** e for e in range(9, 16, 2))
    legend_range = ("o", "s", "^", "*")
    color_range = ("g", "y", "m", "r")
    runtime_keys = [(length, process_range) for length in length_range]
    runtime_dict = {runtime_key: list() for runtime_key in runtime_keys}
    speedup_vector = list()
    extension = os.path.splitext(output)[-1]

    if not extension:
        raise ValueError("The output must have a valid file extension")

    plt.title("Speedup Graph")
    plt.xticks(process_range)
    plt.yticks(process_range)
    plt.xlabel("Number of Processes", fontsize="large")
    plt.ylabel(r"Speedup ($T_1$ / $T_p$)", fontsize="large")
    # The format for axis range is [xmin, xmax, ymin, ymax].
    plt.axis([0, process_range[-1] + 2, 0, process_range[-1] + 2])
    # The Linear Speedup Reference Line
    plt.plot(process_range, process_range,
             color="c", label="Linear", linestyle="--",
             marker="+", markersize=10)

    for length, legend, color in zip(length_range, legend_range, color_range):
        argument_dict["length"] = length
        speedup_vector.clear()
        for process_count in process_range:
            argument_dict["process"] = process_count
            command = program.format(**argument_dict).split()
            # Let 'psrs' program write the moving average and standard error in
            # binary form, rather than the human-readable text form, because
            # 'printf' cannot print exact values of floating-point numbers that
            # easily.
            # 'psrs' calls 'fwrite' to write the moving average and standard
            # error into the 'subprocess.PIPE', and is parsed by the 'unpack'
            # mechanism.

            # The method 'communicate' returns a tuple of the form
            # (stdout_data, stderr_data)
            # here only the first element is of interest.

            # The result of 'unpack' method call is a tuple regardless of the
            # data to be unpacked; since the output of 'psrs' are two
            # double floating-point values, only the first two elements
            # are needed.
            with subprocess.Popen(command, stdout=subprocess.PIPE) as proc:
                mean_time, std_err = struct.unpack("dd", proc.communicate()[0])
            if 1 != process_count:
                # Speedup = T1 / Tp
                speedup = speedup_vector[0] / mean_time
            else:
                speedup = mean_time
            speedup_vector.append(speedup)
            runtime_dict[(length, process_range)].append([mean_time, std_err])

        # The speedup for the 1 process case is always 1
        # set outside the inner loop because all the speedup values in
        # the 'speedup_vector' need to be calculated based on the T1
        speedup_vector[0] = 1.0
        plt.plot(process_range, speedup_vector,
                 color=color, label=_log2_exponent_get(length), linestyle="--",
                 marker=legend, markersize=10)

    plt.legend(loc="best", title="Length")
    plt.savefig(output)
    plt.clf()
    return runtime_dict


def runtime_tabulate(runtime: Dict[RunTimeKey, List[ValPair]], output: str):
    """
    Tabulates mean sorting time with number of processes as x axis (row) and
    length of array as y axis (column).

    NOTE: Assumes all the values in 'runtime' is of same length; so there
    are same number of processes tested for each length.
    """
    if not (isinstance(runtime, dict) and isinstance(output, str)):
        raise TypeError("'runtime' and 'output' need to be of 'dict', 'str'"
                        " types, respectively")

    length_range = [float(key[0]) for key in sorted(runtime.keys())]
    length_labels = [_log2_exponent_get(length) for length in length_range]
    process_range = random.choice(list(runtime.keys()))[-1]
    process_labels = list()
    runtime_matrix = [runtime[key] for key in sorted(runtime.keys())]
    # standard errors are not needed, so an extra step
    # is needed to discard them
    runtime_matrix = [[j[0] for j in i] for i in runtime_matrix]
    runtime_format = [["{0:f}".format(j) for j in i] for i in runtime_matrix]

    for process in process_range:
        label = "{0} Process{1}".format(process, "" if 1 == process else "es")
        process_labels.append(label)

    # plt.axis("tight")
    plt.axis("off")
    plt.title("Sorting Time in Moving Average (second)")
    table = plt.table(cellText=runtime_format,
                      rowLabels=length_labels,
                      colLabels=process_labels,
                      loc="center")
    # table.set_fontsize("large")
    # table.scale(1.2, 1.2)
    table.scale(1, 4.5)
    # figure = plt.gcf()
    # figure.set_size_inches(10, 6)
    plt.savefig(output)
    plt.clf()


def runtime_plot(runtime: Dict[RunTimeKey, List[ValPair]], output: str):
    """
    Plots the runtime using a 3-D bar chart with number of processes and length
    of array as categorical variables.

    Reference:
    http://matplotlib.org/examples/mplot3d/bars3d_demo.html
    """
    if not (isinstance(runtime, dict) and isinstance(output, str)):
        raise TypeError("'runtime' and 'output' need to be of 'dict', 'str'"
                        " types, respectively")

    color_range = ("g", "y", "m", "r")
    length_range = [float(key[0]) for key in sorted(runtime.keys())]
    length_labels = [_log2_exponent_get(length) for length in length_range]
    # Make each group (in terms of length of array in this case) evenly spaced
    length_arrange = [i for i in range(len(length_range))]
    process_range = random.choice(list(runtime.keys()))[-1]
    process_labels = [str(i) for i in process_range]
    # Make each group (in terms of number of processes) evenly spaced
    process_arrange = [i for i in range(len(process_range))]
    runtime_matrix = [runtime[key] for key in sorted(runtime.keys())]
    # standard errors are not needed, so an extra step
    # is needed to discard them
    runtime_matrix = [[j[0] for j in i] for i in runtime_matrix]
    extension = os.path.splitext(output)[-1]
    iterate = zip(runtime_matrix, length_arrange, length_labels, color_range)

    if not extension:
        raise ValueError("The 'output' must have a valid file extension")

    fig = plt.figure()
    ax = fig.add_subplot(111, projection="3d")
    ax.set_xlabel("Number of Processes")
    ax.set_ylabel("Length of Array")
    ax.set_zlabel("Sorting Time")
    plt.title("Sorting Time Per Group")
    plt.xticks(process_arrange, process_labels)
    plt.yticks(length_arrange, length_labels)

    for vector, length, label, color in iterate:
        ax.bar(process_arrange,
               vector,
               zs=length,
               zdir="y",
               color=color,
               alpha=0.5)

    # fig.set_size_inches(10, 6)
    plt.savefig(output)
    plt.clf()


def stdev_tabulate(runtime: Dict[RunTimeKey, List[ValPair]], output: str):
    """
    Tabulates standard deviation of sorting time with number of processes as x
    axis (row) and length of array as y axis (column).

    NOTE: Assumes all the values in 'runtime' is of same length; so there
    are same number of processes tested for each length.
    """
    if not (isinstance(runtime, dict) and isinstance(output, str)):
        raise TypeError("'runtime' and 'output' need to be of 'dict', 'str'"
                        " types, respectively")

    length_range = [float(key[0]) for key in sorted(runtime.keys())]
    length_labels = [_log2_exponent_get(length) for length in length_range]
    process_range = random.choice(list(runtime.keys()))[-1]
    process_labels = list()
    runtime_matrix = [runtime[key] for key in sorted(runtime.keys())]
    # mean sorting times are not needed, so an extra step
    # is needed to discard them
    runtime_matrix = [[j[-1] for j in i] for i in runtime_matrix]
    runtime_format = [["{0:f}".format(j) for j in i] for i in runtime_matrix]

    for process in process_range:
        label = "{0} Process{1}".format(process, "" if 1 == process else "es")
        process_labels.append(label)

    # plt.axis("tight")
    plt.axis("off")
    plt.title("Standard Deviation for Sorting Time")
    table = plt.table(cellText=runtime_format,
                      rowLabels=length_labels,
                      colLabels=process_labels,
                      loc="center")
    # table.set_fontsize("large")
    # table.scale(1.2, 1.2)
    table.scale(1, 4.5)
    # figure = plt.gcf()
    # figure.set_size_inches(10, 6)
    plt.savefig(output)
    plt.clf()


def phase_pie_plot(program: str, length: int, output: str):
    """
    Plots a per-phase running time pie chart based on the 'length' given.

    NOTE:
    Number of processes is hard-coded as 4.

    Reference:
    http://matplotlib.org/examples/pie_and_polar_charts/pie_demo_features.html
    http://stackoverflow.com/questions/19852215/
        how-to-add-a-legend-to-matplotlib-pie-chart
    """
    if not all(map(isinstance, (program, length, output), (str, int, str))):
        raise TypeError("'program', 'length', 'output' must be of "
                        " 'str' 'int' 'str' type, respectively")

    if not shutil.which(program):
        raise ValueError("'program' is not found")

    if not shutil.which("mpiexec"):
        raise ValueError("'mpiexec' is not found")

    phase_time = [None] * 4
    phase_percent = None
    total_time = None
    process = 4
    mpi_prefix = "mpiexec -n {process} "
    # use '-p' command line flag to let 'psrs' prorgram return per-phase time
    psrs_flags = " -b -p -l {length} -r {run} -s {seed} -w {window}"
    program = mpi_prefix + program + psrs_flags
    argument_dict = dict(length=length,
                         process=process,
                         run=1,
                         seed=10,
                         window=1)
    color_range = ["yellowgreen", "gold", "lightskyblue", "lightcoral"]
    explode_range = (0.1, 0, 0, 0)
    phase_labels = ["Phase " + str(i) for i in range(1, 5)]
    length_label = _log2_exponent_get(length)
    title = ("Per-Phase Runtime "
             "(Array Length = {0}, "
             "Number of Processes = {1})").format(length_label, process)

    command = program.format(**argument_dict).split()
    with subprocess.Popen(command, stdout=subprocess.PIPE) as proc:
        # The method 'communicate' returns a tuple of the form
        # (stdout_data, stderr_data)
        # here only the first element is of interest.
        phase_time[0], phase_time[1], phase_time[2], phase_time[3] = \
            struct.unpack("dddd", proc.communicate()[0])

    total_time = sum(phase_time)
    phase_percent = [phase / total_time * 100 for phase in phase_time]

    plt.title(title)
    plt.pie(phase_percent,
            explode=explode_range,
            colors=color_range,
            autopct="%1.1f%%",
            shadow=True,
            startangle=90)
    plt.axis("equal")
    plt.legend(phase_labels, loc="best")
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
    Main command line driver.
    """

    parser = argparse.ArgumentParser()
    attr_desc_dict = {
        "deviation": "file name of sorting time standard deviation table",
        "executable": "path to the PSRS executable",
        "pie": "base file name of pie chart for per-phase sorting time",
        "speedup": "file name of the speed-up plot",
        "table": "file name of the running time summary table",
        "runtime": "3-d bar chart of the running time summary"
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
        runtime_dict = speedup_plot(args.executable, args.speedup)
        runtime_tabulate(runtime_dict, args.table)
        stdev_tabulate(runtime_dict, args.deviation)
        runtime_plot(runtime_dict, args.runtime)
        pie_base, pie_base_ext = os.path.splitext(args.pie)
        if not pie_base_ext or "." == pie_base_ext:
            raise ValueError("'{pie}' must have a "
                             "proper extension".format(args.pie))
        phase_pie_plot(args.executable, 2 ** 21, pie_base + "0" + pie_base_ext)
        phase_pie_plot(args.executable, 2 ** 27, pie_base + "1" + pie_base_ext)
# -------------------------------- FUNCTIONS ----------------------------------


if __name__ == "__main__":
    main()
