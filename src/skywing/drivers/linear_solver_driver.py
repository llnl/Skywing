import argparse
import os
import shutil
from pathlib import Path

from skywing.drivers.driver import add_driver_default_args, run_collective
from skywing.drivers.utils.argparse_utils import argparse_list_to_kwargs
from skywing.drivers.utils.matrix_gen import (
    add_argparse_matrix_options,
    argparse_matrix_options_to_kwargs,
    matrix_gen,
)
from skywing.drivers.utils.plot_convergence import plot_convergence

# Path to the Skywing/python/skywing/drivers directory.
ROOT_DIR = Path(__file__).resolve().parents[0]


def main():
    """Generate a linear system (or read from file), partition and solve over a Skywing collective, then
    produce plots of the convergence and final solution.
    """

    # Parse command line arguments
    parser = argparse.ArgumentParser(
        prog="Skywing linear solver driver",
        description="Create and run a Skywing collective to solve a linear system. "
        "The collective may be defined either by a JSON config file (see ../tests/test_configs for examples) "
        "OR by the num_agents, starting_port, and comm_topology options (passing a config file will override "
        "these options). Additionally, artificial slow-downs may be applied to agents via the test_async option. "
        "This driver also generates and partitions the linear system (or reads it from file) "
        "and produces several plots of the convergence and final solution.",
        epilog="Example: python linear_solver_driver.py SGD",
    )
    parser.add_argument(
        "algorithm",
        type=str,
        help="Algorithm to run (e.g. Jacobi, SGD, etc.). See processors in skywing/mid for available algorithms.",
    )
    parser.add_argument(
        "--output_dir",
        type=Path,
        default=Path("ls_driver_output"),
        help="Output directory where the problem info and iterate history are saved.",
    )
    parser.add_argument(
        "--plot_dir",
        type=Path,
        default=Path("ls_driver_plots"),
        help="Output directory where plots are saved.",
    )
    parser.add_argument(
        "--no_plots",
        action="store_true",
        help="Don't generate plots.",
    )
    add_driver_default_args(parser)
    add_argparse_matrix_options(parser)
    args = parser.parse_args()
    mat_kwargs = argparse_matrix_options_to_kwargs(args)

    # Use ls_driver_subprocess.py as the script
    args.script = str(ROOT_DIR / "utils/ls_driver_subprocess.py")

    # Setup additional kwargs to pass to the subprocess
    kwargs = argparse_list_to_kwargs(args.kwargs)
    kwargs["algorithm"] = args.algorithm
    kwargs["partitioning"] = args.partitioning
    kwargs["output_dir"] = args.output_dir
    if args.lam is not None:
        kwargs["lam"] = args.lam

    # Remove output directory if it exists
    if os.path.exists(args.output_dir):
        shutil.rmtree(args.output_dir)

    # Generate the problem
    matrix_gen(**mat_kwargs)

    # Run the Skywing collective
    err = run_collective(
        args.script,
        config=args.config,
        num_agents=args.num_agents,
        starting_port=args.starting_port,
        comm_topology=args.comm_topology,
        print_to_screen=args.print,
        unbuffered=args.unbuffered,
        test_async=args.test_async,
        profile=args.profile,
        **kwargs,
    )

    # Plot outputs
    if not args.no_plots and not sum(err):
        plot_convergence(
            [args.output_dir],
            args.plot_dir,
            lam=args.lam or 0.0,
            shift_scale=args.shift_scale,
            run_names=[args.algorithm],
        )


if __name__ == "__main__":
    main()
