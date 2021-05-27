from scipy.io import mmread
from numpy import linalg as LA
import numpy as np
from pathlib import Path
import math

def vector_norm_from_file(filename):
    vec=mmread(filename)
    return LA.norm(vec)


def obtain_sync_array(filename, dir):
    # sync_hold_array = np.loadtxt(sync_comparison_results_directory + sync_comparison_results_filename , delimiter=",", skiprows=1)
    # filename= "sync_results_rank*.csv"
    single_experiment_hold = np.empty(7, dtype=float)
    row_wise_partial_vec_norms=[3,4]
    averaged_cols = [5,6]
    for file in dir.glob(filename):
        load_file_array = np.loadtxt(file, delimiter=",", skiprows=1)
        single_experiment_hold = np.vstack((single_experiment_hold, load_file_array))
    print("single_experiment_hold: ")
    print(single_experiment_hold)
    # This deletes the first row of the single experiment hold array.
    single_experiment_hold=np.delete(single_experiment_hold,0,0)
    # Computes the various means. Note thie overwrite a matrix with a vector which contains norms of its columns
    single_experiment_hold_mean = single_experiment_hold[:,averaged_cols]
    single_experiment_hold_mean = single_experiment_hold_mean.mean(axis=0)
    single_experiment_hold_vec_norms = single_experiment_hold[:, row_wise_partial_vec_norms]
    single_experiment_hold_vec_norms = np.sum(single_experiment_hold_vec_norms,axis=0)
    single_experiment_hold_vec_norms[0] = math.sqrt(single_experiment_hold_vec_norms[0])
    single_experiment_hold_vec_norms[1] = math.sqrt(single_experiment_hold_vec_norms[1])
    print(single_experiment_hold_vec_norms)
    print(single_experiment_hold_mean)
    # This saves the mean data for graphing. (Might change)
    # Current format: [ redundancy | trial | averaged local error | averaged local residual | averaged count | averaged time ]
    single_experiment_hold = np.hstack(([0, 1], single_experiment_hold_vec_norms, single_experiment_hold_mean))

    return single_experiment_hold
