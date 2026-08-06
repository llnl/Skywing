from argparse import ArgumentParser
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.animation import FuncAnimation
from plot_utils import configure_matplotlib, construct_iterate_history


# Create animation function
def create_animation(iterates, exact, output_dir, output_filename):
    num_nodes = iterates.shape[1]
    time_steps = iterates.shape[0]
    fig, ax = plt.subplots()
    x = np.arange(num_nodes)  # Node indices
    (line,) = ax.plot(x, iterates[0, :], "b-", label="Solution")
    ax.plot(x, exact, "k:", label="Exact")

    ax.set_xlim(0, num_nodes - 1)
    ax.set_xlabel("Index")
    ax.set_ylabel("Solution Value")
    ax.set_title("Solution Progress Over Time")
    ax.legend()

    def update(frame):
        ymin = min(iterates[frame, :].min(), exact.min())
        ymax = max(iterates[frame, :].max(), exact.max())
        ax.set_ylim(ymin, ymax)
        line.set_ydata(iterates[frame, :])
        return (line,)

    ani = FuncAnimation(fig, update, frames=time_steps, interval=50, blit=True)

    if output_dir:
        output_dir.mkdir(exist_ok=True, parents=True)
    ani.save(f"{output_dir}/{output_filename}.gif", fps=20, writer="pillow")
    plt.close()


if __name__ == "__main__":
    parser = ArgumentParser(
        description="Create a video showing progress of the solution over time."
    )

    parser.add_argument(
        "--output_dir",
        type=Path,
        default=Path("."),
        help="Directory where output is saved",
    )
    parser.add_argument(
        "--plot_dir",
        type=Path,
        default=Path("ls_driver_plots"),
        help="Output directory where plots are saved.",
    )

    # Parse command line arguments
    args = parser.parse_args()

    # Configure matplotlib
    configure_matplotlib()

    # Construct global iterate history and corresponding right-hand sides
    time, iterates, _, _ = construct_iterate_history(args.output_dir)
    b_exact = np.loadtxt(args.output_dir / "b.txt")
    A = np.loadtxt(args.output_dir / "A.txt")
    x_exact = np.loadtxt(args.output_dir / "x.txt")
    b_iterates = np.array([A @ x for x in iterates])

    # Create animation
    create_animation(iterates, x_exact, args.plot_dir, "solution_progress")
    create_animation(b_iterates, b_exact, args.plot_dir, "rhs_progress")

    print(f"Animation saved as {args.plot_dir}/solution_progress.gif")
