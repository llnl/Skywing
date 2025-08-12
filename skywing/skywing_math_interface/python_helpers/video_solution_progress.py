import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from argparse import ArgumentParser
from pathlib import Path
from plot_utils import (
    add_plotting_arguments,
    configure_matplotlib,
    history_filename,
    load_or_construct_global_iterate_history,
    compute_global_solution,
)

# Argument parser setup
def parse_arguments():
    parser = ArgumentParser()
    parser.add_argument(
        '--base_dir', '-b', type=Path, default=Path('.'),
        help='Base directory containing matrix, rhs, and partition information'
    )
    parser.add_argument(
        '--history_dir', type=Path, default=Path('.'),
        help='Directory containing history'
    )
    parser.add_argument(
        '--output_dir', type=Path, default=Path('.'),
        help='Directory where output is saved'
    )
    parser.add_argument(
        '--overwrite_history', '-o', action='store_true',
        help='Overwrite file containing global iterate history (if it exists)'
    )
    add_plotting_arguments(parser)
    return parser.parse_args()

# Main function
def main():
    args = parse_arguments()

    # Configure matplotlib
    configure_matplotlib(args.output_suffix)

    # Load or construct global iterate history
    history_file = history_filename(args.history_dir)
    if args.overwrite_history:
        history_file.unlink(missing_ok=True)

    # WM: todo - this should be the column partition for COLA? Different algs will have different behavior...
    time, iterates = load_or_construct_global_iterate_history(
        args.base_dir / 'row_partition.txt', history_file=history_file
    )

    # Compute global solutions
    solutions = compute_global_solution(iterates, args.base_dir)

    # Create animation
    create_animation(solutions, solutions.shape[1], solutions.shape[0], args.output_dir)

    print(f"Animation saved as {args.output_dir}/solution_progress.gif")

# Create animation function
def create_animation(solutions, num_nodes, time_steps, output_dir):
    fig, ax = plt.subplots()
    x = np.arange(num_nodes)  # Node indices
    line, = ax.plot(x, solutions[0, :], 'b-', label="Solution")

    ax.set_xlim(0, num_nodes - 1)
    ax.set_ylim(solutions.min(), solutions.max())
    ax.set_xlabel("Node")
    ax.set_ylabel("Solution")
    ax.set_title("Solution Progress Over Time")
    ax.legend()

    def update(frame):
        line.set_ydata(solutions[frame, :])
        return line,

    ani = FuncAnimation(fig, update, frames=time_steps, interval=50, blit=True)

    if output_dir:
        output_dir.mkdir(exist_ok=True, parents=True)
    ani.save(f"{output_dir}/solution_progress.gif", fps=20, writer='pillow')

if __name__ == "__main__":
    main()
