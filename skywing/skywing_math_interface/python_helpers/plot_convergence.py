from argparse import ArgumentParser
import matplotlib.pyplot as pyplot
from pathlib import Path
from plot_utils import *

# Parse command line arguments
parser = ArgumentParser()
parser.add_argument('--base_dir', '-b', type=Path, default=Path('.'),
  help='base directory containing matrix, rhs, and partition information')
parser.add_argument('--history_dir', type=Path, default=Path('.'),
  help='directory containing history')
parser.add_argument('--output_dir', type=Path, default=Path('.'),
  help='directory where output is saved')
parser.add_argument('-l', '--lam', type=float, default=None,
  help='l2 regularization.')
parser.add_argument('--shift_scale', type=int, default=0,
  help='whether to shift/scale data matrix')
add_plotting_arguments(parser)
args = parser.parse_args()

# Configure matplotlib settings
configure_matplotlib(args.output_suffix)

# Construct global iterate history
time, global_iterates, agent_local_timestamps, agent_local_iterates = construct_iterate_history(args.history_dir)

# Read in local error metrics
agent_error_metrics = read_error_metrics_file(args.history_dir)

# Initlize figure list
f_list = []

# Read in problem data
A, b_exact, x_exact = read_problem(args.base_dir, shift_scale=args.shift_scale)
rel_res_exact = np.linalg.norm(A @ x_exact - b_exact) / np.linalg.norm(b_exact)

# Compute the global l2 errors and residuals
error, residual = compute_global_error(A, b_exact, x_exact, global_iterates, lam=args.lam)

# Linear plot of l2 convergence
f, ax = pyplot.subplots()
ax.plot(time-time[0], error, '-o')
ax.set_xlabel('Time')
ax.set_ylabel('l2 error')
f_list.append((f, 'linear_convergence'))

# Log plot of l2 convergence
f, ax = pyplot.subplots()
ax.semilogy(time-time[0], error, '-o')
ax.set_xlabel('Time')
ax.set_ylabel('l2 error')
f_list.append((f, 'log_convergence'))

# Plot final solution vs. exact
f, ax = pyplot.subplots()
ax.plot(global_iterates[-1,:], '-', label='Final Solution')
ax.plot(x_exact, 'k:', label='Exact Solution')
ax.set_xlabel('Index')
ax.set_ylabel('Solution Value')
ax.legend(bbox_to_anchor=(1.05, 1), loc=2, borderaxespad=0.)
f_list.append((f, 'final_soln'))

# Linear plot of residual convergence
f, ax = pyplot.subplots()
ax.plot(time-time[0], residual, '-o')
if (rel_res_exact > 0.01):
    ax.plot(time-time[0], rel_res_exact * np.ones(len(residual)), ':k')
ax.set_xlabel('Time')
ax.set_ylabel('Relative residual')
f_list.append((f, 'linear_res_convergence'))

# Log plot of residual convergence
f, ax = pyplot.subplots()
ax.semilogy(time-time[0], residual, '-o')
if (rel_res_exact > 0.01):
    ax.semilogy(time-time[0], rel_res_exact * np.ones(len(residual)), ':k')
ax.set_xlabel('Time')
ax.set_ylabel('Relative residual')
f_list.append((f, 'log_res_convergence'))

# Plot final rhs vs. exact
f, ax = pyplot.subplots()
ax.plot(A @ global_iterates[-1,:], '-', label='Final RHS')
if (rel_res_exact > 0.01):
    ax.plot(A @ x_exact, 'r--', label='A * x_exact')
ax.plot(b_exact, 'k:', label='Exact RHS')
ax.set_xlabel('Index')
ax.set_ylabel('RHS Value')
ax.legend(bbox_to_anchor=(1.05, 1), loc=2, borderaxespad=0.)
f_list.append((f, 'final_rhs'))

# Plot solutions from individual agents
f, ax = pyplot.subplots()
for i, a in enumerate(agent_local_iterates):
    cols = list(a[-1].keys())
    vals = list(a[-1].values())
    linestyle = '--'
    if len(cols) == 1:
        linestyle = 'x'
    ax.plot(cols, vals, linestyle, label=f'Agent {i}')
ax.plot(x_exact, 'k:', label='Exact')
ax.set_xlabel('Index')
ax.set_ylabel('Solution Value')
ax.legend(bbox_to_anchor=(1.05, 1), loc=2, borderaxespad=0.)
f_list.append((f, 'agent_solns'))

# Compute and plot l2 error for each agent
errors = compute_agent_error(A, b_exact, x_exact, agent_local_iterates)
f, ax = pyplot.subplots()
for i,error in enumerate(errors):
    ax.semilogy(agent_local_timestamps[i]-time[0], error, '--', label=f'Agent {i}')
ax.set_xlabel('Time')
ax.set_ylabel('l2 error')
ax.legend(bbox_to_anchor=(1.05, 1), loc=2, borderaxespad=0.)
f_list.append((f, 'agent_convergence'))

# Plot local error metrics from each agent
error_labels = agent_error_metrics[0].keys()
for err_label in error_labels:
    f, ax = pyplot.subplots()
    for i, agent in enumerate(agent_error_metrics):
        ax.semilogy(agent[err_label]['time']-time[0], agent[err_label]['value'], '--', label=f'Agent {i}')
    ax.set_xlabel('Time')
    ax.set_ylabel(err_label)
    ax.legend(bbox_to_anchor=(1.05, 1), loc=2, borderaxespad=0.)
    f_list.append((f, 'agent_' + err_label))


# Save all plots
save_plots(f_list, output_folder=args.output_dir, output_prefix=args.output_prefix)
