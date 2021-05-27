# This is the post procesing step for the async jacobi algorithm.
# All files are expected to be in the form:
# | Redundancy | Trial | Rank | Local Error | Local Residual | Count | Time |

import numpy as np
from pathlib import Path
from matplotlib import pyplot as plt
import sys
import math

import async_jacobi_post_processing_helper as helper

dir = Path('.')
n = len(sys.argv)
starting_redundancy = int(sys.argv[1])
ending_redundancy = int(sys.argv[2])+1
size_of_system = int(sys.argv[3])
number_of_trials = int(sys.argv[4])+1
sync_comparison_results_directory = sys.argv[5]
solution_vector_filename = sys.argv[6]
rhs_vector_filename = sys.argv[7]

initial_error_hold = helper.vector_norm_from_file(solution_vector_filename)
print("initial_error_hold:")
print(initial_error_hold)
rhs_norm = helper.vector_norm_from_file(rhs_vector_filename)

# Used for loading in data for every experiment.
single_experiment_hold = np.empty(7, dtype=float)
# Holds all data for eac set of experiments according to redundancy.
all_trials_one_redundancy_hold = np.empty(6, dtype=float)
# Holds all of the data for the comparison results
all_single_experiment_results = np.empty(6, dtype=float)
# Stores all of the raw data for the sake of completeness.
all_experiment_data = np.empty(7, dtype=float)
# Stores all of the data for comparison results.
redundancy_experiment_results = np.empty(6, dtype=float)

# This vector is used for graphing the comparison results with HD18 with the following form:
# | redundancy | (Averaged) log10 Global Error | (Averaged) log10 Global Residual | (Averaged)
# Count | (Averaged) Time | mean update time | observable rate of convergence | Boost | asyn effect | asyn/sync effect ratio | speedup |
comparison_results_hd = np.empty(11, dtype=float)


row_wise_partial_vec_norms=[3,4]
averaged_cols = [5,6]
averaged_cols_all = [2,3,4,5]

for redundancy in range(starting_redundancy, ending_redundancy):
    for trial in range(1, number_of_trials):
        filename= "data_hold/redundancy_" + str(redundancy) + "_trial_" + str(trial) + "_rank_*.csv"
        for file in dir.glob(filename):
            load_file_array = np.loadtxt(file, delimiter=",", skiprows=1)
            single_experiment_hold = np.vstack((single_experiment_hold, load_file_array))
        # This deletes the first row of the single experiment hold array.
        single_experiment_hold=np.delete(single_experiment_hold,0,0)
        # Computes the various means. Note thie overwrite a matrix with a vector which contains norms of its columns
        single_experiment_hold_mean = single_experiment_hold[:, averaged_cols]
        single_experiment_hold_mean = single_experiment_hold_mean.mean(axis=0)
        single_experiment_hold_vec_norms = single_experiment_hold[:, row_wise_partial_vec_norms]
        single_experiment_hold_vec_norms = np.sum(single_experiment_hold_vec_norms,axis=0)
        single_experiment_hold_vec_norms[0] = math.sqrt(single_experiment_hold_vec_norms[0])/redundancy
        single_experiment_hold_vec_norms[1] = math.sqrt(single_experiment_hold_vec_norms[1])/redundancy
        # This saves the mean data for graphing. (Might change)
        # Current format: [ redundancy | trial | averaged local error | averaged local residual | averaged count | averaged time ]
        single_experiment_hold_mean = np.hstack(([redundancy, trial], single_experiment_hold_vec_norms, single_experiment_hold_mean))
        all_trials_one_redundancy_hold = np.vstack((all_trials_one_redundancy_hold, single_experiment_hold_mean))
        # print(all_trials_one_redundancy_hold.shape)
        all_single_experiment_results = np.vstack((all_single_experiment_results, single_experiment_hold_mean))
        # This saves all data from all experiments.
        all_experiment_data = np.vstack((all_experiment_data, single_experiment_hold))
        # Resets the hold array.
        single_experiment_hold = np.empty(7, dtype=float)

    all_trials_one_redundancy_hold = np.delete(all_trials_one_redundancy_hold,0,0)
    redundancy_experiment_results_hold = all_trials_one_redundancy_hold[:,averaged_cols_all]
    redundancy_experiment_results_hold = redundancy_experiment_results_hold.mean(axis=0)
    redundancy_experiment_results_hold = np.hstack(([redundancy, trial],redundancy_experiment_results_hold))
    redundancy_experiment_results = np.vstack((redundancy_experiment_results, redundancy_experiment_results_hold))
    all_trials_one_redundancy_hold = np.empty(6, dtype=float)


# Deletes the first row created by the empty array
all_single_experiment_results = np.delete(all_single_experiment_results,0,0)
all_experiment_data = np.delete(all_experiment_data,0,0)
redundancy_experiment_results = np.delete(redundancy_experiment_results,0,0)

# # Syntax for multiplying list.
# a_list = [1, 2, 3]
# multiplied_list = [element * 2 for element in a_list]


print("Single Experiment Results:")
print(all_single_experiment_results)
print("All Experiment Date:")
print(all_experiment_data)
print("Averaged Redundancy Experiment Results")
print(redundancy_experiment_results)

# Obtains the sync results for comparisons.
sync_comparison_results_filename = "redundancy_1_trial_1_rank_*.csv"
sync_hold_array = helper.obtain_sync_array(sync_comparison_results_directory + sync_comparison_results_filename, dir)
print("Sync hold array:")
print(sync_hold_array)

# This makes the save directory for all data files and plots.
save_directory = "results/"
Path(save_directory).mkdir(parents=True, exist_ok=True)

# This piece saves the raw data for every experiment.
experiment_name = save_directory + "master_file_raw_data_redundancy_" + str(starting_redundancy) + "_to_" + str(ending_redundancy) + "_trials_"+ str(trial) + ".csv"
np.savetxt(experiment_name, all_experiment_data, delimiter=",",newline='\n')
# Data from which each experiment is averaged among processes; there is data for each single trial from each redundancy.
experiment_name = save_directory + "master_file_all_single_experiment_results_redundancy_" + str(starting_redundancy) + "_to_" + str(ending_redundancy) + "_trials_"+ str(trial) + ".csv"
# Data from which each trial for each redundancy is averaged among all processes and trials for same redundancy.
np.savetxt(experiment_name, all_single_experiment_results, delimiter=",",newline='\n')
experiment_name = save_directory + "master_file_consolidated_redundancy_experiment_means_redundancy_" + str(starting_redundancy) + "_to_" + str(ending_redundancy) + "_trials_"+ str(trial) + ".csv"
np.savetxt(experiment_name, redundancy_experiment_results, delimiter=",",newline='\n')


# Plots the async/sync ratios for runtime and global error.
number_of_experiments = ending_redundancy - starting_redundancy
runtime_ratios  = np.empty(number_of_experiments, dtype=float)
global_error_ratios = np.empty(number_of_experiments, dtype=float)

# Obtains the data from the plots from the raw data:
for i in range(0, number_of_experiments):
    runtime_ratios[i] = redundancy_experiment_results[i][5]/sync_hold_array[5]
    global_error_ratios[i] = redundancy_experiment_results[i][2]/sync_hold_array[2]
# print("Runtime Ratios")
# print(runtime_ratios)
# print("Global Error Ratios")
# print(global_error_ratios)

redundancy_counter_x_axis = [i for i in range(starting_redundancy, ending_redundancy)]
figure, axis = plt.subplots(2, 1)

# For Runtime Ratio Plot
axis[0].bar(redundancy_counter_x_axis, runtime_ratios)
axis[0].set_title("Runtime Ratios")
# For Global Error Ratio Plot
axis[1].bar(redundancy_counter_x_axis, global_error_ratios)
axis[1].set_title("Global Error Ratios")
# Recursively set axis titles
for ax in axis.flat:
    ax.set(xlabel='Redundancy', ylabel='Async/Sync Ratio')

plt.savefig(save_directory + 'experiment_name.png')
# plt.show()


# This block explicitly computes the comparison results as defined in Hook and Dingle.

sync_comparison_array_hd = sync_hold_array[2:6];
sync_comparison_array_hd[0] = math.log(sync_comparison_array_hd[0],10)
sync_comparison_array_hd[1] = math.log(sync_comparison_array_hd[1],10)
# Appends update rate.
sync_update_rate = sync_comparison_array_hd[2]/sync_comparison_array_hd[3]
sync_comparison_array_hd = np.append(sync_comparison_array_hd, [sync_update_rate], axis=0)
# Appends Observable rate of convergence.
sync_observable_rate_of_convergence = (math.log(initial_error_hold,10) -  sync_comparison_array_hd[0])/sync_comparison_array_hd[3]
sync_comparison_array_hd = np.append(sync_comparison_array_hd, [sync_observable_rate_of_convergence], axis=0)
# Appends sync effect.
sync_effect = sync_observable_rate_of_convergence/sync_update_rate
sync_comparison_array_hd = np.append(sync_comparison_array_hd, [sync_effect], axis=0)
print("Synchronous Results for Comparison:")
print(sync_comparison_array_hd)



for i in range(0, number_of_experiments):
    comparison_results_hd_hold = np.array(i)
    comparison_results_hd_hold = np.append(comparison_results_hd_hold,redundancy_experiment_results[i,2:6])
    comparison_results_hd_hold[1] = math.log(comparison_results_hd_hold[1],10)
    comparison_results_hd_hold[2] = math.log(comparison_results_hd_hold[2],10)
    # Appends update rate.
    async_update_rate = comparison_results_hd_hold[3]/comparison_results_hd_hold[4]
    comparison_results_hd_hold = np.append(comparison_results_hd_hold, [async_update_rate], axis=0)
    # Appends Observable rate of convergence.
    async_observable_rate_of_convergence = (math.log(initial_error_hold,10) -  comparison_results_hd_hold[1])/comparison_results_hd_hold[4]
    comparison_results_hd_hold = np.append(comparison_results_hd_hold, [async_observable_rate_of_convergence], axis=0)
    # Appends Boost
    boost = async_update_rate/sync_update_rate
    comparison_results_hd_hold = np.append(comparison_results_hd_hold, [boost], axis=0)
    # Appends sync effect.
    async_effect = async_observable_rate_of_convergence/async_update_rate
    comparison_results_hd_hold = np.append(comparison_results_hd_hold, [async_effect], axis=0)
    effect_ratio = async_effect/sync_effect
    comparison_results_hd_hold = np.append(comparison_results_hd_hold, [effect_ratio], axis=0)
    speedup = async_observable_rate_of_convergence/sync_observable_rate_of_convergence
    comparison_results_hd_hold = np.append(comparison_results_hd_hold, [speedup], axis=0)
    comparison_results_hd = np.vstack((comparison_results_hd, comparison_results_hd_hold))
comparison_results_hd = np.delete(comparison_results_hd,0,0)
experiment_name = save_directory + "async_sync_comp_results_+hd18_redundancy" + str(starting_redundancy) + "_to_" + str(ending_redundancy) + "_trials_"+ str(trial) + ".csv"
np.savetxt(experiment_name, comparison_results_hd, delimiter=",",newline='\n')
print("Async/Sync Comparison Results HD:")
print(comparison_results_hd)
# # Basic Plot Schematic
# plt.figure()
# plt.plot(xdata, ydata, [params])
# plt.xlabel('Redundancy (hr)')
# plt.ylabel('Position (km)')
