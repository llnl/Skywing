@defgroup admm ADMM
# ADMM for solving a linear least squares optimization problem

**NOTE**: There is another example using ADMM leveraging the standardized `LinearSystemDriver` functionality
for running iterative methods for linear systems, located in `examples/math_interface_examples/admm`.
It demonstrates using ADMM as a synchronous or asynchronous method, as well as different
data layout schemes for the linear system.

## Overview
This example demonstrates using the decentralized Alternating Direction Method of
Multipliers (ADMM) implemented in the Skywing framework for solving a linear least squares optimization problem.
It uses a decentralized consensus approach to solve min sum{i=1}^L (1/2) ||A_ix - b_i||_2^2.

The example has the option for solving two different problems: one based on a 14-bus IEEE problem set-up
(fixed size and network size) and one for determining an unknown signal where the user inputs dictate
the size of the resulting linear least squares problem.

## Problem options
Both example problems are intended to demonstrate how consensus ADMM
can be used to solve a linear least squares optimization problem.

### 14-bus
The motivation for this example is a power grid with M observable measurements (power) corresponding
to N state variables (voltage), where the vector of power measurements is denoted by z in R^M and
vector of state variables by x in R^N. Assume the power grid has more observable quantities than state
variables (M > N) and the observable quantities have a linear dependence on the state variables.
The grid state-estimation model is then expressed as Hx=z, where H in R^{MxN} is the matrix of admittance values.
To solve, the model is cast as an optimization problem: min sum{i=1}^M (1/2) ||H_ix - z_i||_2^2.

This example sets up the communication pattern given in "Distributed Robust Power System State Estimation", Kekatos and Giannakis, 2013,
for the IEEE 14-bus transmission system partitioned into 4 control areas, where each agent is responsible for an area. However,
this example adds a line and sensor in Area 4 between buses 10 and 14 (to improve the conditioning of local linear problem).
The least squares optimization problem is divided into the 4 local problems, one for each area of the power grid.
Based on the partition of the state and observational variables across the 4 areas where we assume the state vector
corresponds to bus voltage values so it is of size 14 and the measurement vector corresponds to line current values
from 18 sensors within the system. This simplified example does not use actual electrical values, only the
network topology and locations of sensors to determine the linear system, but aims to demonstrate
setting up a real-world example for using ADMM.

The example reads the input communication partition, as well as the partitioned linear system,
measurements, and exact solution from input files
located within the `14_bus_problem_data` directory.

### random-signal
In this problem, we consider the problem of estimating an unknown signal (state),
from noisy measurements collected across a network of L agents.

In particular, we assume a network of L agents where each agent i owns a local dataset
A_i, the linear measurement matrix of agent i whose elements follow N(0,1), and assume
the measurement vector b_i of agent i is polluted by random noise. That it,
b_i = A_i x + e_i where e_i ~ N(0, noise*I).

It is assumed that a line communication topology is used so each agent (except agent 0)
subscribes to information to the agent "to their right".

The goal is to find a common parameter x in R^N goal is to find the common state x
by minimizing the sum-of-squares loss min_{x} sum{i=1}^L (1/2) ||A_ix - b_i||_2^2.

where

- x is the true state (unknown signal) to estimate, x in R^N,
- A_i in R^M_i x N is the local measurement matrix,
- b_i in R^M_i is the noisy measurement (observational quantity).

#### Inputs for random-ls-example problem

| Parameter              | Option                         | Default |
|------------------------|--------------------------------|---------|
| Number of agents       | --size_of_network (L)          | 4       |
| State dimension        | --state_dim (N)                | 5       |
| Observations per agent | --observations_per_agent (M_i) | 100     |
| Noise variance         | --noise (noise)                | 0.01    |

## Run the example

- To run the example, execute `source run.sh [starting_port_number] options` in the build directory.
- See `source run.sh --help` for a full list of inputs.
- *NOTE*: The option `--problem 14-bus` will use 4 agents and disregards all other input parameters,
  compared to using `--problem random-signal` which will give the user more control to alter the input listed above.
- Each agent will output its local solution
  as well as the relative L2 error compared to the exact solution.