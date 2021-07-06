import numpy as np
from pathlib import Path
from scipy.io import mmread
from scipy.io import mmwrite
import sys

# These first two functions are for diagnostics.
#
def print_linear_system (A,x,b):
    [m,n] = np.shape(A)
    for i in range(m):
        if i != np.floor(m/2):
            print(A[i,:], x[i], "   ", b[i])
        else:
            print(A[i,:], x[i], " = ", b[i])

def system_check(A,x_sol,b, name_of_system,  system_hold_folder):
    # This is a check if the linear system has been generated correctly.
    Ax = np.matmul(A,x_sol)
    residual = np.linalg.norm(Ax - b)

    if (residual > 10-6):
        print("System is NOT accurate  -> ||Ax - b|| = " , residual)
        if np.prod(np.shape(b)) < 10:
            b_expected = np.matmul(A,x_sol)
            print("Expected b:\n",b_expected)
            x_sol_new= np.linalg.lstsq(A, b, rcond=None)[0]
            print("Expected x:\n",x_sol_new)
        error_from_input = np.linalg.norm(x_sol_new - x_sol)
        residual_from_input = np.linalg.norm(b-b_expected)
        print("||x_input - x_least_squares||: " , error_from_input)
        print("||Ax - b||: " , residual_from_input)
        filename = system_hold_folder + "/" + "x_sol_new_" +  name_of_system
        mmwrite(filename, x_sol_new)
        filename = system_hold_folder + "/" + "b_expected_" +  name_of_system
        mmwrite(filename, b_expected)
    else:
        print("System is accurate  -> ||Ax - b|| = " , residual)

dir = Path('.')
n = len(sys.argv)

size_of_system = int(sys.argv[1])
name_of_system = sys.argv[2]
system_hold_folder = sys.argv[3]
partition_hold_folder = sys.argv[4]
row_count = 1


A_filename = system_hold_folder + "/" + name_of_system
A=mmread(A_filename)
for machine_number in range(0,size_of_system):
    row_hold = A[machine_number,:]
    row_hold=row_hold[None,:]
    A_partition_filename = system_hold_folder + "/" + partition_hold_folder + "/machine_" + str(machine_number) + "_row_count_" + str(row_count)  + "_" + name_of_system
    mmwrite(A_partition_filename, row_hold)


b_filename = system_hold_folder + "/" + "rhs_" +  name_of_system
b = mmread(b_filename)
for machine_number in range(0,size_of_system):
    row_hold=b[machine_number]
    row_hold=row_hold[:,None]
    b_partition_filename = system_hold_folder + "/" + partition_hold_folder + "/machine_" + str(machine_number) + "_row_count_" + str(row_count) + "_rhs_" + name_of_system
    mmwrite(b_partition_filename, row_hold)


x_sol_filename = system_hold_folder + "/" + "x_sol_" +  name_of_system
x_sol = mmread(x_sol_filename)
for machine_number in range(0,size_of_system):
    row_hold=x_sol[machine_number]
    row_hold=row_hold[:,None]
    x_sol_partition_filename = system_hold_folder + "/" + partition_hold_folder + "/machine_" + str(machine_number) + "_row_count_" + str(row_count) + "_x_sol_" + name_of_system
    mmwrite(x_sol_partition_filename,row_hold)

index_filename = system_hold_folder + "/" + "indices_" +  name_of_system
indices = mmread(index_filename)
for machine_number in range(0,size_of_system):
    row_hold=indices[machine_number]
    row_hold=row_hold[:,None]
    indices_partition_filename = system_hold_folder + "/" + partition_hold_folder + "/machine_" + str(machine_number) + "_row_count_" + str(row_count) + "_indices_" + name_of_system
    mmwrite(indices_partition_filename,row_hold)

# This checks if the linear system is correct.
system_check(A,x_sol,b, name_of_system,  system_hold_folder)
