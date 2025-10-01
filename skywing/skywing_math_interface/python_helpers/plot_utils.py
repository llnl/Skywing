from typing import Callable, List, Tuple, Optional
from matplotlib import pyplot as plt
import numpy as np
from pathlib import Path
import sys
import logging
from scipy import sparse

def read_history_file(input_file):
    '''
    Read history file returning the timestamps and iterates.
    Assumed history file format (each row): timestamp \t [ (index, value) (index, value) ...  ]
    NOTE - assume indexing is done by integers from 0...N with each index represented on at least one agent
    '''

    # Initialize the lists
    local_iterates = []
    local_timestamps = []

    # Open the file
    with open(input_file, 'r') as f:

        # Loop over lines
        for line in f:

            # Strip whitespace and split the line into timestamp and elements
            line = line.strip()
            timestamp, elements = line.split('\t')

            # Loop over elements
            iterate = {}
            for elm in elements.split(') ('):
                index, value = elm.strip('[]() ').split(',')
                iterate[int(index)] = float(value)

            # Accumulate info
            local_iterates.append(iterate)
            local_timestamps.append(int(timestamp))

    return local_timestamps, local_iterates

def read_error_metrics_file(history_dir=Path('.')) -> List[dict]:
    '''
    Read in error metric file. The resulting agent_error_metrics is a list
    of dictionaries for each agent, where each error_metrics dictionary has the form:
    error_metrics[label] = { 'time' : [list of timestamps], 'value' : [list of values] }
    Assumed error metrics file format (each row): timestamp \t label \t value  ]
    '''

    # Loop over history files
    agent_error_metrics = []
    while 1:
        try:
            # Initialize the error_metrics dict
            error_metrics = {}
            # Open the file
            input_file = history_dir / f'error_metrics{len(agent_error_metrics)}.txt'
            with open(input_file, 'r') as f:
                # Loop over lines
                for line in f:
                    # Strip whitespace and split the line into timestamp, label, and value
                    line = line.strip()
                    timestamp, label, value = line.split('\t')
                    # Add to error_metrics dict
                    if not error_metrics.get(label):
                        error_metrics[label] = {'time' : [], 'value' : []}
                    error_metrics[label]['time'].append(int(timestamp))
                    error_metrics[label]['value'].append(float(value))
            # Convert to numpy arrays
            for label in error_metrics.keys():
                error_metrics[label]['time'] = np.array(error_metrics[label]['time'])
                error_metrics[label]['value'] = np.array(error_metrics[label]['value'])
            # Append to list over agents
            agent_error_metrics.append(error_metrics)
        except FileNotFoundError:
            print(f'Read in {len(agent_error_metrics)} error metric files.')
            break

    return agent_error_metrics

def add_plotting_arguments(parser):
    plot_output_group  = parser.add_argument_group('Plotting Output', description='Arguments used related to plotting. If saved, files will be saved to "{output_folder}/{output_prefix}{name}{output_suffix}" where output_suffix is implemented by matplotlib configuration. See `save_plots` and `configure_matplotlib` for more info.')
    plot_output_group.add_argument('--output_prefix', type=str, default='', help='Prefix to prepend to output filename, likely ending with a human readable separator')
    plot_output_group.add_argument('--output_suffix', type=str, help='Suffix to prepend to output filename (extension/filetype).')

def save_plots(fig_name_pairs: Tuple[plt.Figure, str],  output_folder: Optional[Path] = None, output_prefix: Optional[str] = None):
    '''
    Either show the active plots, or save the passed in figures to (roughly) {output_folder}/{output_prefix}{name}{rcSuffix} for an rcSuffix from configure_matplotlib

    Keyword arguments:
    fig_name_pairs -- When saving: A list of figure/name pairs, where the name should not include a suffix and will be used for saving
    output_folder -- When saving: The folder to save to
    output_prefix -- When saving: A prefix to append to each name (within the folder), should likely end in a human readable separator (e.g. '.','-','_',',')
    '''


    if output_folder:
        output_folder.mkdir(exist_ok=True, parents=True)
    for f, name in fig_name_pairs:
        filename = output_prefix + name if output_prefix else name
        output_file = output_folder/filename if output_folder else Path(filename)

        if '.' in name:
            raise ValueError(f'The name received by save_plots ({name}) has a suffix, which should instead be specified using --output_suffix or a call to configure_matplotlib')
        f.savefig(output_file, dpi=300, bbox_inches="tight")

def configure_matplotlib(output_suffix: str = None) -> None:

    plt.rcParams['text.usetex'] = True
    plt.rcParams['font.size'] = 16.0
    plt.rcParams['legend.fontsize'] = 16.0
    plt.rcParams['figure.figsize'] = 6, 4.5
    if output_suffix:
        plt.rcParams['savefig.format'] = output_suffix.replace('.','')

def construct_iterate_history(history_dir=Path('.')) -> Tuple[np.array, np.ndarray, list, list]:
    '''
    Read in all available history files and construct local and global iterate histories.
    '''

    # Loop over history files to get the local iterates and timestamps from each agent
    # Also record how many agents own a representation of each coordinate in the solution
    agent_local_iterates = []
    agent_local_timestamps = []
    coord_cnts = {}
    num_agents = 0
    while 1:
        try:
            history_file = history_dir / f'history{num_agents}.txt'
            local_timestamps, local_iterates = read_history_file(history_file)
            agent_local_iterates.append(local_iterates)
            agent_local_timestamps.append(local_timestamps)
            for i in local_iterates[0].keys():
                if i in coord_cnts:
                    coord_cnts[i] += 1
                else:
                    coord_cnts[i] = 1
            num_agents += 1
        except FileNotFoundError:
            print(f'Read in {num_agents} history files.')
            break
        except Exception as err:
            break
            logging.error(f'at \'{file}\'' , exc_info=err)
            raise BadHistoryFileException(f'{file}')

    # Unique and sort the list of all timestamps to get the global timestamps
    flattened_timestamps = [t for timestamps in agent_local_timestamps for t in timestamps]
    global_timestamps = list(set(flattened_timestamps))
    global_timestamps.sort()
    global_timestamps = np.array(global_timestamps)

    # Build up the global iterates for all timestamps in the global timestamps
    global_num_timestamps = len(global_timestamps)
    global_num_dofs = len(coord_cnts)
    global_iterates = np.zeros((global_num_timestamps, global_num_dofs))
    np_coord_cnts = np.empty(global_num_dofs)
    for key, val in coord_cnts.items():
        np_coord_cnts[key] = val
    agent_counters = [-1 for a in range(num_agents)]
    for i, t in enumerate(global_timestamps):
        for a in range(num_agents):
            if agent_counters[a] + 1 < len(agent_local_timestamps[a]) and t == agent_local_timestamps[a][ agent_counters[a] + 1 ]:
                agent_counters[a] += 1
            if agent_counters[a] >= 0:
                cols = list(agent_local_iterates[a][ agent_counters[a] ].keys())
                vals = list(agent_local_iterates[a][ agent_counters[a] ].values())
                # Note - in the case where multiple agents share dofs, we take an average for the global iterate
                global_iterates[i, cols] += vals
        global_iterates[i, :] /= np_coord_cnts

    return global_timestamps, global_iterates, agent_local_timestamps, agent_local_iterates

def read_problem(base_dir: Path, shift_scale: bool = False) -> Tuple[np.ndarray, np.array, np.array]:
    '''
    Read in matrix, rhs, and exact solution from file
    '''
    b = np.loadtxt(base_dir / 'b.txt')
    A = np.loadtxt(base_dir / 'A.txt')
    x_exact = np.loadtxt(base_dir / 'x.txt')
    if shift_scale:
        A -= np.sum(A, axis=0) / A.shape[0]
        A /= np.sqrt(np.sum(A * A, axis=0) / A.shape[0])
        b -= np.sum(b) / b.size
        b /= np.sqrt(np.sum(b * b) / b.size)
    return A, b, x_exact

def compute_global_error(A: np.ndarray, b: np.array, x_exact: np.array, global_iterates: np.ndarray, lam: float = None) -> Tuple[np.array, np.array]:
    '''
    Compute the global relative error defined as e_n = ||x_n - x||_2 / ||x||_2
    '''
    if lam:
        A = np.vstack((A, np.sqrt(lam) * np.eye(A.shape[1])))
        b = np.concatenate((b, np.zeros(A.shape[1])))
    l2_errors = np.asarray([
        np.linalg.norm(x - x_exact) / np.linalg.norm(x_exact) for x in global_iterates])
    residuals = np.asarray([
        np.linalg.norm(A @ x - b) / np.linalg.norm(b) for x in global_iterates])
    return l2_errors, residuals

def compute_agent_error(A: np.ndarray, b: np.array, x_exact: np.array, agent_local_iterates: list) -> List[np.array]:
    '''
    Compute the relative error for each agent, defined as
    e_n = ||(x_i)_n - x_i||_2 / ||x_i||_2
    '''
    agent_l2_errors = []
    for a in range(len(agent_local_iterates)):
        l2_errors = []
        for iterate in agent_local_iterates[a]:
            cols = list(iterate.keys())
            vals = list(iterate.values())
            x = np.zeros_like(x_exact)
            x[cols] = vals
            l2_errors.append( np.linalg.norm(x[cols] - x_exact[cols]) / np.linalg.norm(x_exact[cols]) )
        agent_l2_errors.append(l2_errors)
    return agent_l2_errors
