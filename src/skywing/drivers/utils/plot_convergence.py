import argparse
import os
import shutil
from pathlib import Path
from typing import Union

import numpy as np
from matplotlib import pyplot

from skywing.drivers.utils.plot_utils import (
    compute_agent_error,
    compute_global_error,
    configure_matplotlib,
    construct_iterate_history,
    read_error_metrics_file,
    read_problem,
    save_plots,
)


def plot_convergence(
    data_dirs: list[Path],
    plot_dir: Path,
    lam: float = 0.0,
    shift_scale: bool = False,
    run_names: Union[list[str], None] = None,
):
    """Generate several convergence plots and solution/rhs visualizations from
    the history data saved in each directory specified in data_dirs.

    Args:
        data_dirs (list[Path]): list of directories containing history data.
        plot_dir (Path): the directory where the plots will be saved.
        lam (float): regularization parameter for least squares problems.
        shift_scale (bool): whether to shift and scale the matrix and right-hand side.
        run_names (list[str]): optional list of names for the runs associated with each data dir.
    """

    # Configure matplotlib settings
    configure_matplotlib()

    # Remove plot directory if it exists
    if os.path.exists(plot_dir):
        shutil.rmtree(plot_dir)

    # Default run names
    N = len(data_dirs)
    if not run_names:
        run_names = [str(i) for i in range(N)]

    # Construct global iterate history
    time = []
    global_iterates = []
    agent_local_timestamps = []
    agent_local_iterates = []
    for d in data_dirs:
        t, gi, alt, ali = construct_iterate_history(d)
        time.append(t)
        global_iterates.append(gi)
        agent_local_timestamps.append(alt)
        agent_local_iterates.append(ali)

    # Read in local error metrics
    agent_error_metrics = []
    for d in data_dirs:
        a = read_error_metrics_file(d)
        agent_error_metrics.append(a)

    # Read in problem data
    A = []
    b_exact = []
    x_exact = []
    rel_res_exact = []
    for d in data_dirs:
        a, be, xe = read_problem(d, shift_scale=shift_scale)
        A.append(a)
        b_exact.append(be)
        x_exact.append(xe)
        if lam:
            a_lam = np.vstack((a, np.sqrt(lam) * np.eye(a.shape[1])))
            be_lam = np.concatenate((be, np.zeros(a.shape[1])))
            rre = np.linalg.norm(a_lam @ xe - be_lam) / np.linalg.norm(be_lam)
        else:
            rre = np.linalg.norm(a @ xe - be) / np.linalg.norm(be)
        rel_res_exact.append(rre)

    # Compute the global l2 errors and residuals
    error = []
    residual = []
    for i in range(N):
        e, r = compute_global_error(
            A[i], b_exact[i], x_exact[i], global_iterates[i], lam=lam
        )
        error.append(e)
        residual.append(r)

    # Initlize figure list
    f_list = []

    # Linear plot of l2 convergence
    f, ax = pyplot.subplots()
    for i in range(N):
        ax.plot(time[i] - time[i][0], error[i], label=run_names[i])
    ax.set_xlabel("Time")
    ax.set_ylabel("l2 error")
    ax.legend(bbox_to_anchor=(1.05, 1), loc=2, borderaxespad=0.0)
    f_list.append((f, "linear_convergence"))

    # Log plot of l2 convergence
    f, ax = pyplot.subplots()
    for i in range(N):
        ax.semilogy(time[i] - time[i][0], error[i], label=run_names[i])
    ax.set_xlabel("Time")
    ax.set_ylabel("l2 error")
    ax.legend(bbox_to_anchor=(1.05, 1), loc=2, borderaxespad=0.0)
    f_list.append((f, "log_convergence"))

    # Plot final solution vs. exact
    f, ax = pyplot.subplots()
    for i in range(N):
        ax.plot(global_iterates[i][-1, :], "-", label=run_names[i] + " Solution")
        ax.plot(x_exact[i], ":k")
    ax.set_xlabel("Index")
    ax.set_ylabel("Solution Value")
    ax.legend(bbox_to_anchor=(1.05, 1), loc=2, borderaxespad=0.0)
    f_list.append((f, "final_soln"))

    # Linear plot of residual convergence
    f, ax = pyplot.subplots()
    for i in range(N):
        ax.plot(time[i] - time[i][0], residual[i], label=run_names[i])
        if rel_res_exact[i] > 0.01:
            ax.plot(
                time[i] - time[i][0], rel_res_exact[i] * np.ones(len(residual[i])), ":k"
            )
    ax.set_xlabel("Time")
    ax.set_ylabel("Relative residual")
    ax.legend(bbox_to_anchor=(1.05, 1), loc=2, borderaxespad=0.0)
    f_list.append((f, "linear_res_convergence"))

    # Log plot of residual convergence
    f, ax = pyplot.subplots()
    for i in range(N):
        ax.semilogy(time[i] - time[i][0], residual[i], label=run_names[i])
        if rel_res_exact[i] > 0.01:
            ax.semilogy(
                time[i] - time[i][0], rel_res_exact[i] * np.ones(len(residual[i])), ":k"
            )
    ax.set_xlabel("Time")
    ax.set_ylabel("Relative residual")
    ax.legend(bbox_to_anchor=(1.05, 1), loc=2, borderaxespad=0.0)
    f_list.append((f, "log_res_convergence"))

    # Plot final rhs vs. exact
    f, ax = pyplot.subplots()
    for i in range(N):
        ax.plot(A[i] @ global_iterates[i][-1, :], "-", label=run_names[i] + " Ax")
        if rel_res_exact[i] > 0.01:
            ax.plot(A[i] @ x_exact[i], "--r")
        ax.plot(b_exact[i], ":k")
    ax.set_xlabel("Index")
    ax.set_ylabel("RHS Value")
    ax.legend(bbox_to_anchor=(1.05, 1), loc=2, borderaxespad=0.0)
    f_list.append((f, "final_rhs"))

    # Plot solutions from individual agents
    for i in range(N):
        f, ax = pyplot.subplots()
        for j, a in enumerate(agent_local_iterates[i]):
            cols = list(a[-1].keys())
            vals = list(a[-1].values())
            linestyle = "--"
            if len(cols) == 1:
                linestyle = "x"
            ax.plot(cols, vals, linestyle, label=f"Agent {j}")
        ax.plot(x_exact[i], "k:", label="Exact")
        ax.set_xlabel("Index")
        ax.set_ylabel("Solution Value")
        ax.legend(bbox_to_anchor=(1.05, 1), loc=2, borderaxespad=0.0)
        f_list.append((f, "agent_solns_" + run_names[i].replace(".", "_")))

    # Compute and plot l2 error for each agent
    for i in range(N):
        errors = compute_agent_error(
            A[i], b_exact[i], x_exact[i], agent_local_iterates[i]
        )
        f, ax = pyplot.subplots()
        for j, error in enumerate(errors):
            ax.semilogy(
                agent_local_timestamps[i][j] - time[i][0],
                error,
                "--",
                label=f"Agent {j}",
            )
        ax.set_xlabel("Time")
        ax.set_ylabel("l2 error")
        ax.legend(bbox_to_anchor=(1.05, 1), loc=2, borderaxespad=0.0)
        f_list.append((f, "agent_convergence_" + run_names[i].replace(".", "_")))

    # Plot local error metrics from each agent
    for i in range(N):
        if len(agent_error_metrics[i]):
            error_labels = agent_error_metrics[i][0].keys()
            for err_label in error_labels:
                f, ax = pyplot.subplots()
                for j, agent in enumerate(agent_error_metrics[i]):
                    ax.semilogy(
                        agent[err_label]["time"] - time[0],
                        agent[err_label]["value"],
                        "--",
                        label=f"Agent {j}",
                    )
                ax.set_xlabel("Time")
                ax.set_ylabel(err_label)
                ax.legend(bbox_to_anchor=(1.05, 1), loc=2, borderaxespad=0.0)
                f_list.append(
                    (f, "agent_" + err_label + "_" + run_names[i].replace(".", "_"))
                )

    # Save all plots
    save_plots(f_list, output_folder=plot_dir)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Create convergence plots.")

    parser.add_argument(
        "--data_dirs",
        type=Path,
        nargs="+",
        default=[Path(".")],
        help="Directory where output is saved",
    )
    parser.add_argument(
        "--plot_dir",
        type=Path,
        default=Path("ls_driver_plots"),
        help="Output directory where plots are saved.",
    )
    parser.add_argument("--lam", type=float, default=None, help="l2 regularization.")
    parser.add_argument(
        "--shift_scale", type=int, default=0, help="whether to shift/scale data matrix"
    )
    parser.add_argument(
        "--run_names",
        type=str,
        nargs="+",
        default=None,
        help="Run names for each data dir",
    )

    # Parse command line arguments
    args = parser.parse_args()

    # Make the plots
    plot_convergence(
        args.data_dirs, args.plot_dir, args.lam, args.shift_scale, args.run_names
    )
