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
    compute_global_error,
    compute_partition_error,
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

    time, iterates = load_or_construct_global_iterate_history(
        args.base_dir / 'partition.txt', history_file=history_file
    )

    # Compute and plot global error
    global_error = compute_global_error(iterates, args.base_dir)

    plot_error(time, global_error, 'linear_convergence', scale='linear')
    plot_error(time, global_error, 'log_convergence', scale='log')

    # Compute and plot partition error
    partition_errors = compute_partition_error(iterates, args.base_dir)
    plot_partition_error(time, partition_errors)

    # Compute global solutions
    solutions = compute_global_solution(iterates, args.base_dir)

    # Create animation
    create_animation(solutions, len(partition_errors), len(partition_errors[0]))

    print("Animation saved as 'solution_progress.gif'")

# Plot error function
def plot_error(time, error, filename, scale='linear'):
    f, ax = plt.subplots()
    if scale == 'linear':
        ax.plot(time - time[0], error, '-o')
    elif scale == 'log':
        ax.semilogy(time - time[0], error, '-o')
    else:
        raise ValueError("Invalid scale. Use 'linear' or 'log'.")
    
    ax.set_xlabel('Time')
    ax.set_ylabel('Solution L2 Error')
    f.savefig(f"{filename}.png")
    plt.close(f)

# Plot partition error function
def plot_partition_error(time, errors):
    f, ax = plt.subplots()
    for i, error in enumerate(errors):
        ax.semilogy(time - time[0], error, '--', label=f'Partition {i}')
    ax.set_xlabel('Time')
    ax.set_ylabel('Solution L2 Error')
    ax.legend(loc='best')
    f.savefig("partition_convergence.png")
    plt.close(f)

# Create animation function
def create_animation(solutions, num_nodes, time_steps):
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
    ani.save("solution_progress.gif", fps=20, writer='pillow')

if __name__ == "__main__":
    main()