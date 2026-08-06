# Generic driver

The driver implemented in `driver.py` is designed to create a Skywing collective
in which each agent runs a generic python script. It is intended for running
a Skywing collective on a single machine and spawns python subprocesses for each
Skywing agent, automatically generating the appropriate command line arguments
for each agent providing their agent ID, address, port, and neighbor connections.
In addition, one can introduce artificial slow-downs in order to emulate highly
asynchronous environments (see the `--test_asyn` command line option).

The python script run by each agent subprocess is suggested to utilize the utility
functions from `skywing/drivers/utils`, which contain standard approaches for things
like setting up command line arguments (`parse_driver_command_line` and
`argparse_list_to_kwargs`), creating the Skywing agent (`create_skywing_agent`), and
ingesting data (`CSVDataLoader` and `LinearSystemDataLoader`). This saves some replication
of boilerplate code.

For convenience, upon installation via pip, Skywing provides the command `skywing_drive`
as an alias for running `driver.py`.

## Examples:

WM: todo - add a simpler and more generic example than `simple_iteration.py` (as part of a tutorial?)

The `simple_iteration.py` example is designed to be compatiple with `driver.py` and
is a convenient script for simple stand-alone runs of Skywing algorithms. The
processor to run is specified by the keyword argument `processor`. Thus, a
minimal command line to run the Skywing `Max` processor and print output to
screen would be:

`python driver.py ../../examples/simple_iteration.py --kwargs processor=Max --print`

The above runs a default Skywing collective consisting of 3 agents in a fully
connected communication topology. To run the `Max` processor in a collective with
5 agents with a ring communication topology, run:

`python driver.py ../../examples/simple_iteration.py --kwargs processor=Max --print --num_agents 5 --comm_topology ring`

Processor-specific keyword arguments may also be passed via the driver to
`simple_iteration.py`, for example:

`python driver.py ../../examples/simple_iteration.py --print --kwargs processor=SGD data_type=matrix partitioning=row gamma=0.1`

In the command above, the `partitioning` and `gamma` keyword arguments are specific
to the `SGD` processor, and get passed along to the processor constructor. Note that
in addition, we specify `data_type=matrix`, which is an script-specific keyword
argument specifying that `simple_iteration.py` should construct matrix data (instead
of the default scalar data), which is what the `SGD` processor expects as input.

# Linear solver driver

The linear solver driver implemented in `linear_solver_driver.py` uses the same
generic driver functionality from `driver.py`, but specializes to the workflow
of solving linear systems. This driver generates a linear system and partitioning
among agents, reads and solves the system on a Skywing collective, saves outputs
from each agent to file, then postprocesses these outputs to generate convergence
plots. See the help string for `linear_solver_driver.py` for detailed info on
all command line arguments.

## Examples:

A minimal command line to run the `SGD` processor would be:

`python linear_solver_driver.py SGD`

Just like the generic driver, the configuration of the Skywing collective may be
modified via `--num_agents`, `--starting_port`, and `--comm_topology`, or by
providing a config file via `--config`. Also, much like `driver.py` additional
keyword arguments may be passed along to the underlying algorithms, for example:

`python linear_solver_driver.py SGD --kwargs gamma=0.1`

There are many options available for generating specific types of matrices or
reading matrices from file. See the `linear_solver_driver.py` help string
for a full list. Note that the parameters refer to the global matrix, that is
specifying `--n_rows` and `--n_cols` will generate a global matrix of that
size that is then partitioned (as equally as possible) among the given number
of agents. The partitioning can be given by the `--partitioning` option and
must be `row` (default) or `col`. Note that the partitioning chosen for the
matrix is also passed along as a keyword argument to the underlying processor
(many but not all processors support both row and column partitioned variants).
Similarly, the l2 regularization term for least squares problems specified by
the `--lam` option will also be passed along to processors that require this
information. Thus, a command for running the `SONATA` processor for a column-
partitioned problem where each of the three agents owns a single column with
10 rows, a regularization, `lam`, of 0.001, and processor-specific parameter
`gamma` of 0.1 would be:

`python linear_solver_driver.py SONATA --partition col --lam 0.001 --n_rows 10 --n_cols 3 --kwargs gamma=0.1`
