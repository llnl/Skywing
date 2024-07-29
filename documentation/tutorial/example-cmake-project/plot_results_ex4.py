import numpy as np
from scipy.interpolate import interp1d
import matplotlib.pyplot as plt
import sys

def print_help():
    print("Plot the results of the multiple_runs Skywing test.\n")
    print("Usage:")
    print("% python3 plot_results results_filename")

if len(sys.argv) != 2 or sys.argv[1] == "--help":
    print_help()
    exit()


output_dicts = {}
output_names = ["TotalChargeEstimate", "BatteryChargeSensor", "ReceivedTotalCharge", "Flow"]
for name in output_names:
    output_dicts[name] = {}
    
values_dict = {}
sensor_dict = {}
state_dict = {}
alert_dict = {}

outfile = open(sys.argv[1], 'r')

for line in outfile:
    tokens = line.split()

    try:
        time = float(tokens[1].strip('s,'))
        
        agent = int(tokens[3].strip(','))
        for name in output_names:
            if agent not in output_dicts[name]:
                output_dicts[name][agent] = []

        data_name = tokens[4].strip(':')
        value = float(tokens[5])
        value = value if np.abs(value) < 10**6 else 0.0
        output_dicts[data_name][agent].append((time, value))
    except:
        pass

value_limits = [-5, 5]

fig, ax = plt.subplots(2,2)

# Plot values data
for agent, arrays in output_dicts["TotalChargeEstimate"].items():
    time_array, values_array = zip(*arrays)
    ax[0][0].plot(time_array, list(values_array), label="Agent" + str(agent))
    #ax[0][0].set_ylim(value_limits)
ax[0][0].legend()
ax[0][0].set_title("TotalChargeEstimates")

# Compute sum-of-sensor data
time_min = min([arrays[0][0] for agent, arrays in output_dicts["BatteryChargeSensor"].items()])
time_max = min([arrays[-1][0] for agent, arrays in output_dicts["BatteryChargeSensor"].items()])
time_common = np.linspace(time_min, time_max, 200)
sensor_common = False
for agent, arrays in output_dicts["BatteryChargeSensor"].items():
    time_array, values_array = zip(*arrays)
    interp = interp1d(time_array, values_array, bounds_error=False, fill_value="extrapolate")
    if sensor_common is False:
        sensor_common = interp(time_common)
    else:
        sensor_common = sensor_common + interp(time_common)

# Plot sensor data, along with true sums
for agent, arrays in output_dicts["BatteryChargeSensor"].items():
    time_array, sensor_array = zip(*arrays)
    ax[0][1].plot(time_array, sensor_array, ".", label="Agent" + str(agent))
    #ax[0][1].set_ylim(value_limits)
ax[0][1].plot(time_common, sensor_common, "k--", label="Sum")
ax[0][1].legend()
ax[0][1].set_title("BatteryChargeSensors")

# Plot state data
for agent, arrays in output_dicts["ReceivedTotalCharge"].items():
    time_array, state_array = zip(*arrays)
    ax[1][0].plot(time_array, state_array, label="Agent" + str(agent))
    #ax[1][0].set_ylim(value_limits)
ax[1][0].plot(time_array, [400.0] * len(time_array), '-k')
ax[1][0].plot(time_array, [-400.0] * len(time_array), '-k')
ax[1][0].plot(time_array, [300.0] * len(time_array), '--k')
ax[1][0].plot(time_array, [-300.0] * len(time_array), '--k')
ax[1][0].legend()
ax[1][0].set_title("ReceivedTotalCharge")

# Plot alert data
for agent, arrays in output_dicts["Flow"].items():
    time_array, alert_array = zip(*arrays)
    ax[1][1].plot(time_array, alert_array, label="Agent" + str(agent))
    #ax[1][1].set_ylim([-0.1, 1.1])
ax[1][1].legend()
ax[1][1].set_title("Control Actions")

fig.suptitle(sys.argv[1])
plt.show()
