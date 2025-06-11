from typing import Callable, List, Tuple, Optional
from matplotlib import pyplot as plt
import numpy as np
from pathlib import Path
import sys
import logging
from scipy import sparse

class BadHistoryFileException(Exception):
  pass

class EmptyHistoryFileException(Exception):
  pass

algorithm_labels = {'jacobi': 'ASJ'}

COLORS = plt.get_cmap('tab10')
MARKERS = ['.', '+', 'x', 'd']

def transform_file(input_file, output_file):
    with open(input_file, 'r') as infile, open(output_file, 'w') as outfile:
        for line in infile:
            # Strip whitespace and split the line
            line = line.strip()
            if line:  # Check if the line is not empty
                # Split the line into parts
                parts = line.split('\t')
                if len(parts) > 1:
                    # Extract the first part and the second part
                    first_part = parts[0]
                    # Extract the number from the second part
                    second_part = parts[1].strip('[]() ').split(',')[1]
                    # Write the transformed line to the output file
                    outfile.write(f"{first_part} {second_part}\n")

def add_plotting_arguments(parser):
    plot_output_group  = parser.add_argument_group('Plotting Output', description='Arguments used related to plotting. If saved, files will be saved to "{output_folder}/{output_prefix}{name}{output_suffix}" where output_suffix is implemented by matplotlib configuration. See `save_plots` and `configure_matplotlib` for more info.')
    plot_output_group.add_argument('--output_folder_name', type=Path, default=Path('.'),
      help='Folder to store output')
    plot_output_group.add_argument('--output_prefix', type=str, default='',
      help='Prefix to prepend to output filename, likely ending with a human readable separator')
    plot_output_group.add_argument('--output_suffix', type=str,
      help='Suffix to prepend to output filename (extension/filetype).')
    
def save_plots(fig_name_pairs: Tuple[plt.Figure, str],  output_folder: Optional[Path] = None, output_prefix: Optional[str] = None):
    """Either show the active plots, or save the passed in figures to (roughly) {output_folder}/{output_prefix}{name}{rcSuffix} for an rcSuffix from configure_matplotlib

    Keyword arguments:
    fig_name_pairs -- When saving: A list of figure/name pairs, where the name should not include a suffix and will be used for saving
    output_folder -- When saving: The folder to save to
    output_prefix -- When saving: A prefix to append to each name (within the folder), should likely end in a human readable separator (e.g. '.','-','_',',')
    """


    if output_folder:
        output_folder.mkdir(exist_ok=True, parents=True)
    for f, name in fig_name_pairs:
      filename = output_prefix + name if output_prefix else name
      output_file = output_folder/filename if output_folder else Path(filename)

      if '.' in name:
          raise ValueError(f'The name received by save_plots ({name}) has a suffix, which should instead be specified using --output_suffix or a call to configure_matplotlib')
      f.savefig(output_file)

def configure_matplotlib(output_suffix: str = None) -> None:

  plt.rcParams['text.usetex'] = True
  plt.rcParams['font.size'] = 16.0
  plt.rcParams['legend.fontsize'] = 16.0
  plt.rcParams['figure.figsize'] = 6, 4.5
  if output_suffix:
    plt.rcParams['savefig.format'] = output_suffix.replace('.','')

def history_filename(output_dir=Path('.')) -> Path:
  return output_dir / 'history.npy'

def read_partition(filename: Path):
  '''Create a dictionary containing the partition file information.

     Returns a list with the i-th entry being a list of rows local to node i:

       partition[i] = list of rows local to node i
  '''
  with open(filename) as f:
    lines = f.read().splitlines()
  partition = []
  for line in lines:
    partition.append([])
    for word in line.split():
      partition[-1].append(int(word))

  return partition

def construct_global_iterate_history(partition_file: Path,
  history_dir=Path('.'), partial_data: bool = False) -> Tuple[np.array, np.ndarray]:
  '''Create the global iterate history for each timestamp, using partition
     information.

     Returns a tuple (t,x) where x is the global iterate at time t
  '''
  # create two dictionaries such that
  #   iterate_dict[<timestamp>] = <iterate vector> that occured at <timestamp>
  #   node_dict[<timestamp>] = which node the <timestamp> came from
  # using history0.txt, history1.txt, ..., history{N-1}.txt

  # AF Note: This needs to be generalized. Right now its assuming one element per agent! 

  partition = read_partition(partition_file)
  N = len(partition)
  print(" --- N = ", N)
  iterate_dict = {}
  node_dict = {}
  for n in range(N):
    try:
      file_temp = history_dir / f'history{n}.txt'
      file = history_dir / f'history_new{n}.txt'
      transform_file(file_temp, file)

      if partial_data:
        with open(file, 'r') as f:
          initial_data = f.readline()
          for line in f:
              pass
          final_data = line
          initial_data = np.fromstring(initial_data, sep=' ', dtype=np.float64)
          final_data = np.fromstring(final_data, sep=' ', dtype=np.float64)
          history_from_node_n : np.ndarray = np.vstack([initial_data, final_data])
      else:
          history_from_node_n : np.ndarray = np.loadtxt(file, ndmin=2)
      if len(history_from_node_n) == 0:
        raise EmptyHistoryFileException(f'{file}')
    except Exception as err:
      logging.error(f'at \'{file}\'' , exc_info=err)
      raise BadHistoryFileException(f'{file}')
    for row in history_from_node_n:


      if row[0] not in iterate_dict.keys():
        elements = partition[n]
        iterate_dict[row[0]] = np.zeros(N) 
        iterate_dict[row[0]][elements] = (row[1:])
        node_dict[row[0]] = [n]
      else:
        elements = partition[n]
        iterate_dict[row[0]][elements] = (row[1:])
        node_dict[row[0]].append(n)
        logging.debug(f"WARN: Merged conflicting updates at time {row[0]} from nodes {node_dict[row[0]]}")

  # construct global iterate history from iterate_dict and node_dict
  time = sorted(iterate_dict.keys())
  if partial_data:
    # initial and final iterates only
    global_iterates = np.empty((2, len(iterate_dict[time[0]])))
  else:
    # all iterates
    global_iterates = np.empty((len(time), len(iterate_dict[time[0]])))
  # set initial global iterate from first timestamp
  t_0 = time[0]
  global_iterates[0] = iterate_dict[t_0]
  # update global iterate only from those iterate elements that were local to
  # the corresponding node
  if partial_data:
    # Backward for-loop in iterates, until one update is recorded for each agent
    recorded = np.zeros(N, dtype=bool)
    for n in range(len(time)-2, -2, -1):
        t_np1 = time[n+1]
        for node_np1 in node_dict[t_np1]:
          recorded[node_np1] = True
        if all(recorded):
            begin_idx = n
            break
    # Forward for-loop in iterates, until all updates are performed in the second row
    global_iterates[1] = np.copy(global_iterates[0])
    for n in range(begin_idx, len(time)-1):
      t_np1 = time[n+1]
      for node_np1 in node_dict[t_np1]:
        elements_np1 = partition[node_np1]
        global_iterates[1][elements_np1] = iterate_dict[t_np1][elements_np1]
    time = [time[0], time[-1]]
  else:
    # Forward for-loop in iterates, until all updates are performed in a new row
    for n in range(len(time)-1):
      t_np1 = time[n+1]
      global_iterates[n+1] = np.copy(global_iterates[n])
      for node_np1 in node_dict[t_np1]:
        elements_np1 = partition[node_np1]
        global_iterates[n+1][elements_np1] = iterate_dict[t_np1][elements_np1]

  return np.array(time), np.array(global_iterates)

def save_timeseries(time: np.array, data: np.ndarray, filename: Path):
  time_nd = np.row_stack(time)
  stacked = np.hstack((time_nd, data))
  np.save(filename, stacked)

def load_timeseries(filename: Path, partial_data: bool = False) -> Tuple[np.array, np.ndarray]:
  stacked = np.load(filename, mmap_mode='r')
  if partial_data:
    time = stacked[[0, -1], 0]
    data = stacked[[0, -1], 1:]
  else:
    time = stacked[:, 0]
    data = stacked[:, 1:]
  return time, data

def load_or_construct_global_iterate_history(partition_file: Path,
  history_file=Path('history.npy'), partial_data: bool = False) -> Tuple[np.array, np.ndarray]:
  '''Load global iterate history from file, if file exists, or construct the
     history and then write to file'''
  if (history_file.exists()):
    print(f'Loading global iterate history from {history_file}...')
    time, iterates = load_timeseries(history_file, partial_data)
  else:
    print(f'Constructing global iterate history and writing to {history_file}...')
    
    time, iterates = construct_global_iterate_history(partition_file,
      history_dir=history_file.parent, partial_data=partial_data)
    save_timeseries(time, iterates, history_file)
  return time, iterates

def compute_global_error(global_iterates: np.ndarray, base_dir: Path) -> np.array:
  '''Compute the global relative error defined as e_n = ||x_n - x||_2 / ||x||_2'''
  b = np.loadtxt(base_dir / 'rhs.txt')
  try:
    A = np.loadtxt(base_dir / 'laplacian.txt')
    x_exact = np.linalg.solve(A, b)
  except:
    import scipy.sparse.linalg as splinalg
    A = load_sparse_matrix_file(base_dir / 'laplacian.txt')
    x_exact = splinalg.spsolve(A, b)
  errors = np.asarray([
    np.linalg.norm(x - x_exact) / np.linalg.norm(x_exact)
      for x in global_iterates])
  return errors

def compute_global_solution(global_iterates: np.ndarray, base_dir: Path) -> np.array:
  '''Compute the global relative error defined as e_n = ||x_n - x||_2 / ||x||_2'''
  solutions = np.asarray([
   x 
      for x in global_iterates])
  return solutions

def compute_partition_error(global_iterates: np.ndarray, base_dir: Path) -> List[np.array]:
  '''Compute the relative error for each partition, defined as
     e_n = ||(x_i)_n - x_i||_2 / ||x_i||_2'''
  b = np.loadtxt(base_dir / 'rhs.txt')
  try:
    A = np.loadtxt(base_dir / 'laplacian.txt')
    x_exact = np.linalg.solve(A, b)
  except:
    import scipy.sparse.linalg as splinalg
    A = load_sparse_matrix_file(base_dir / 'laplacian.txt')
    x_exact = splinalg.spsolve(A, b)
  partition = read_partition(base_dir / 'partition.txt')
  errors = []
  for rows in partition:
    error = np.asarray([
      np.linalg.norm(x[rows] - x_exact[rows]) / np.linalg.norm(x_exact[rows])
        for x in global_iterates])
    errors.append(error)
  return errors



def load_sparse_matrix_file(filename: Path) -> sparse:
  """Read a file containing sparse matrix data and return a scipy sparse matrix.

  Inputs:
  filename - The file to be read.

  Returns a scipy sparse matrix in CSR format.
  """
  row_indices = []
  col_indices = []
  data = []

  with filename.open('r') as f:
    # TODO: maybe confirm this is "sparse" to indicate a sparse matrix
    f.readline()

    # Number of rows and columns of the matrix on the first line
    num_rows, num_cols = map(int, f.readline().strip().split())

    for line in f:
      # Each row occupies one line, with
      # the row index in the first number,
      # followed by pairs of column index and value.
      elements = line.strip().split()
      row = int(elements[0])
      for col_idx in range(1, len(elements), 2):
        col = int(elements[col_idx])
        value = float(elements[col_idx + 1])
        row_indices.append(row)
        col_indices.append(col)
        data.append(value)

  A = sparse.csr_matrix((data, (row_indices, col_indices)), shape=(num_rows, num_cols))
  return A

