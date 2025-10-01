import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from argparse import ArgumentParser
from pathlib import Path
from plot_utils import *

# Parse command line arguments
def parse_arguments():
    parser = ArgumentParser()
    parser.add_argument('--base_dir', '-b', type=Path, default=Path('.'),
        help='Base directory containing matrix, rhs, and partition information')
    parser.add_argument('--history_dir', type=Path, default=Path('.'),
        help='Directory containing history')
    parser.add_argument('--output_dir', type=Path, default=Path('.'),
        help='Directory where output is saved')
    add_plotting_arguments(parser)
    return parser.parse_args()

# Main function
def main():
    args = parse_arguments()

    # Configure matplotlib
    configure_matplotlib(args.output_suffix)

    # Construct global iterate history and corresponding right-hand sides
    time, iterates, _, _ = construct_iterate_history(args.history_dir)
    b_exact = np.loadtxt(args.base_dir / 'b.txt')
    A = np.loadtxt(args.base_dir / 'A.txt')
    x_exact = np.loadtxt(args.base_dir / 'x.txt')
    b_iterates = np.array([A @ x for x in iterates])

    # Create animation
    create_animation(iterates, x_exact, args.output_dir, "solution_progress")
    create_animation(b_iterates, b_exact, args.output_dir, "rhs_progress")

    print(f"Animation saved as {args.output_dir}/solution_progress.gif")

# Create animation function
def create_animation(iterates, exact, output_dir, output_filename):
    num_nodes = iterates.shape[1]
    time_steps = iterates.shape[0]
    fig, ax = plt.subplots()
    x = np.arange(num_nodes)  # Node indices
    line, = ax.plot(x, iterates[0, :], 'b-', label="Solution")
    ax.plot(x, exact, 'k:', label="Exact")

    ax.set_xlim(0, num_nodes - 1)
    ax.set_xlabel("Index")
    ax.set_ylabel("Solution Value")
    ax.set_title("Solution Progress Over Time")
    ax.legend()

    def update(frame):
        ymin = min(iterates[frame,:].min(), exact.min())
        ymax = max(iterates[frame,:].max(), exact.max())
        ax.set_ylim(ymin, ymax)
        line.set_ydata(iterates[frame, :])
        return line,

    ani = FuncAnimation(fig, update, frames=time_steps, interval=50, blit=True)

    if output_dir:
        output_dir.mkdir(exist_ok=True, parents=True)
    ani.save(f"{output_dir}/{output_filename}.gif", fps=20, writer='pillow')
    plt.close()

if __name__ == "__main__":
    main()
