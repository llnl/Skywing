import argparse


def add_driver_default_args(parser: argparse.ArgumentParser):
    parser.add_argument(
        "--config",
        type=str,
        help="Path to JSON config file specifying the ID, address, port, and neighbors for each agent in the Skywing collective.",
    )
    parser.add_argument(
        "--num_agents",
        type=int,
        default=3,
        help="Number of agents in the Skywing collective",
    )
    parser.add_argument(
        "--starting_port",
        type=int,
        default=20000,
        help="Starting port number (agents in the Skywing collective will be assigned sequential port numbers starting here).",
    )
    parser.add_argument(
        "--comm_topology",
        type=str,
        choices=["full", "ring", "line"],
        default="full",
        help="Communication topology for agents in the Skywing collective.",
    )
    parser.add_argument(
        "-p",
        "--print",
        action="store_true",
        help="Print stdout and stderr from agents to screen.",
    )
    parser.add_argument(
        "-u",
        "--unbuffered",
        action="store_true",
        help="Stream agent output live instead of buffering until completion.",
    )
    parser.add_argument(
        "--test_async",
        type=float,
        nargs=3,
        help="Introduce artificial asynchrony parameterized by [%% slow agents] [%% slow iterations] [slow down in seconds]. \
        Passing a negative value for the %% slow agents yields randomly selected agent IDs (a positive value will deterministically assign slow agents with the lowest agent IDs first). \
        Passing a negative value for the slow down yields a random slow down drawn from a uniform distribution between 0.0 and (-slow_down) seconds (a positive value will apply a deterministic slow down on each slow iteration).",
    )
    parser.add_argument(
        "--profile",
        action="store_true",
        help="Profile agent subprocesses with cProfile.",
    )
    parser.add_argument(
        "--kwargs",
        type=str,
        nargs="+",
        help="Additional key word arguments passed to the script.",
    )


def argparse_list_to_kwargs(argparse_list):
    """Convert the list coming from argparse to a kwargs dictionary.
    input argparse_list = ['param1=val1', 'param2=val2', ...]
    return kwargs = {'param1' : val1, 'param2' : val2, ...}
    """
    kwargs = {}
    if argparse_list is not None:
        for param_val_str in argparse_list:
            # Split into parameter name and value
            param, val = param_val_str.split("=")
            # Cast value to int, float, or retain as string (other types not currently supported)
            try:
                val = int(val)
            except ValueError:
                try:
                    val = float(val)
                except ValueError:
                    pass
            # Add to kwarg dictionary
            kwargs[param] = val

    return kwargs


def parse_driver_command_line():
    parser = argparse.ArgumentParser()
    parser.add_argument("--agent_id", type=int, help="This agent's ID.")
    parser.add_argument("--address", type=str, help="This agent's address.")
    parser.add_argument("--port", type=int, help="This agent's port.")
    parser.add_argument(
        "--nbr_addresses",
        type=str,
        nargs="+",
        help="Addresses for this agent's neighbors.",
    )
    parser.add_argument(
        "--nbr_ports",
        type=int,
        nargs="+",
        help="Ports for this agent's neighbors.",
    )
    parser.add_argument(
        "--test_async",
        type=float,
        nargs=3,
        help="Introduce artificial asynchrony parameterized by [slow agent flag] [%% slow iterations] [slow down in seconds]. \
        Passing a negative value for the slow down yields a random slow down drawn from a uniform distribution between 0.0 and (-slow_down) seconds.",
    )
    parser.add_argument(
        "--kwargs",
        type=str,
        nargs="+",
        help="Additional key word arguments passed to the script.",
    )

    args = parser.parse_args()

    # Generate an agent ID if none is provided
    if args.agent_id is None:
        args.agent_id = args.port

    return args
