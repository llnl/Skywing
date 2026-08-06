# How to run examples

For each example, there are two different ways to run it.

## Method 1: Using `driver.py` to launch agents

Example: `python ../skywing/drivers/driver.py simple_iteration.py --kwargs processor=Max --print`

See the `README.md` file and/or the help string for `driver.py` in `skywing/drivers` for more info.

## Method 2: Launching each agent in a terminal, or on separate machines

Example:

In terminal 1: `python simple_iteration.py --port 20000 --nbr_ports 20001 --kwargs processor=Max`

In terminal 2: `python simple_iteration.py --port 20001 --nbr_ports 20000 --kwargs processor=Max`

This method is useful if you are running agents on separate machines, such as on the Raspberry Pi testbed. 

- Required parameters: 
`--port`
`--nbr_ports`
These are the ports and ports of the neighbors of this agent.

See example executable help strings for info on additional parameters.

# Creating new examples

It is suggested to utilize the utility functions from `skywing/drivers/utils`, which contain
standard approaches for things like setting up command line arguments (`parse_driver_command_line`
and `argparse_list_to_kwargs`), creating the Skywing agent (`create_skywing_agent`), and
ingesting data (`CSVDataLoader` and `LinearSystemDataLoader`). This saves some replication of
boilerplate code across examples and helps to ensure that high-level behavior and structure
are similar between examples.
