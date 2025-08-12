from argparse import ArgumentParser
import matplotlib.pyplot as pyplot
from pathlib import Path
# from run_utils import compute_global_error, compute_partition_error,
from plot_utils import add_plotting_arguments, configure_matplotlib, save_plots, history_filename,load_or_construct_global_iterate_history,compute_global_error,compute_partition_error

parser = ArgumentParser()
parser.add_argument('--base_dir', '-b', type=Path, default=Path('.'),
  help='base directory containing matrix, rhs, and partition information')
parser.add_argument('--history_dir', type=Path, default=Path('.'),
  help='directory containing history')
parser.add_argument('--output_dir', type=Path, default=Path('.'),
  help='directory where output is saved')
parser.add_argument('--overwrite_history', '-o', action='store_true', dest='overwrite_history',
  help='overwrite file containing global iterate history (if it exists)')
add_plotting_arguments(parser)
args = parser.parse_args()



configure_matplotlib(args.output_suffix)

# load / construct global iterate history
historyFile = history_filename(args.history_dir)
print(args.history_dir)
print(historyFile)
if (args.overwrite_history):
  historyFile.unlink(missing_ok=True)

# WM: todo - this should be the column partition for COLA? Different algs will have different behavior...
time, iterates = load_or_construct_global_iterate_history(args.base_dir / 'row_partition.txt', history_file=history_filename(args.history_dir))
print(iterates[0,:])
f_list = []

# compute and plot global error
error = compute_global_error(iterates, args.base_dir)
print(error)

f, ax = pyplot.subplots()
ax.plot(time-time[0], error, '-o')
ax.set_xlabel('time')
ax.set_ylabel('solution L2 error')
f_list.append((f, 'linear_convergence'))

f, ax = pyplot.subplots()
ax.semilogy(time-time[0], error, '-o')
ax.set_xlabel('time')
ax.set_ylabel('solution L2 error')
f_list.append((f, 'log_convergence'))

# compute and plot partition error
errors = compute_partition_error(iterates, args.base_dir)
f, ax = pyplot.subplots()
for i,error in enumerate(errors):
  ax.semilogy(time-time[0], error, '--', label=f'partition {i}')
ax.set_xlabel('time')
ax.set_ylabel('solution L2 error')
ax.legend(loc='best')
f_list.append((f, 'partition_convergence'))

save_plots(f_list, output_folder=args.output_dir, output_prefix=args.output_prefix)
