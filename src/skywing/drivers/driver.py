import argparse
import json
import random
import subprocess
import sys
import threading
from typing import Optional

from skywing.drivers.utils.argparse_utils import (
    add_driver_default_args,
    argparse_list_to_kwargs,
)


def add_nbr_addr_port(collective_config: dict):
    for agent_id in collective_config.keys():
        collective_config[agent_id]["nbr_addresses"] = []
        collective_config[agent_id]["nbr_ports"] = []
        for nbr_id in collective_config[agent_id]["nbrs"]:
            collective_config[agent_id]["nbr_addresses"].append(
                collective_config[nbr_id]["address"]
            )
            collective_config[agent_id]["nbr_ports"].append(
                collective_config[nbr_id]["port"]
            )


def load_collective_config(path):
    """Load the collective configuration from a JSON file.
    Convert the list into a dictionary with agent IDs as keys.
    Add neighbor address/port info to the dictionary.
    """
    # Read JSON file
    with open(path) as f:
        read_config = json.load(f)

    # Create dictionary with agent IDs as keys
    collective_config = {}
    for agent_info in read_config:
        agent_id = agent_info.pop("id")
        collective_config[agent_id] = agent_info

    # Add neighbor address/port info
    add_nbr_addr_port(collective_config)

    return collective_config


def get_nbr_list_from_topo(agent_id: int, num_agents: int, comm_topology: str):
    """Generate the list of neighbors for the given agent_id according to the
    comm_topology and num_agents. Agent IDs are assumed to be sequential integers
    starting at 0, and the supported communication topologies are full, ring, line.
    """
    if comm_topology == "full":
        return [i for i in range(num_agents)]
    elif comm_topology == "ring":
        return [agent_id, (agent_id - 1) % num_agents, (agent_id + 1) % num_agents]
    elif comm_topology == "line":
        if agent_id == 0:
            return [agent_id, agent_id + 1]
        elif agent_id == num_agents - 1:
            return [agent_id, agent_id - 1]
        else:
            return [agent_id, agent_id - 1, agent_id + 1]


def stream_output(prefix, stream):
    """Read lines from a stream and print them with a prefix."""
    for line in stream:
        print(f"{prefix}: {line.rstrip()}", flush=True)
    stream.close()


def run_collective(
    script: str,
    config: str = "",
    num_agents: int = 3,
    starting_port: int = 20000,
    comm_topology: str = "full",
    print_to_screen: bool = False,
    unbuffered: bool = False,
    test_async: Optional[list[float]] = None,
    profile: bool = False,
    **kwargs,
):
    """Launches subprocesses with appropriate command line
    arguments to run as a Skywing collective where each subprocess
    corresponds to a Skywing agent running the given script.
    Note that the collective may be defined in a JSON config file
    OR by the num_agents, starting_port, and comm_topology options.
    Specifying a path to a config file will override other options
    for specifying the collective.

    Args:
        script: path to the script that the agents will run.
        config: path to JSON config file describing the collective.
        num_agents: number of agents.
        starting_port: starting port for agent 0 (ports assigned sequentially).
        comm_topology: the communication topology (full, ring, or line).
        print_to_screen: print the agent outputs to screen at the end of the run.
        unbuffered: print agent outputs in real time.
        test_async: add artificial slow downs [% slow agents, % slow iterations, slow down (s)].
        profile: profile the agent subprocesses
        kwargs: additional kwargs passed on to the agent subprocesses.
    """

    # Read or generate collective configuration
    if config:
        collective_config = load_collective_config(config)
    else:
        collective_config = {}
        for i in range(num_agents):
            collective_config[i] = {}
            collective_config[i]["address"] = "127.0.0.1"
            collective_config[i]["port"] = i + starting_port
            collective_config[i]["nbrs"] = get_nbr_list_from_topo(
                i, num_agents, comm_topology
            )
        add_nbr_addr_port(collective_config)

    processes = []
    threads = []

    # Generate info for slow agents if testing with artificial asynchrony
    if test_async:
        # Get the test_asyn parameters
        percent_slow_agents = test_async[0]
        percent_slow_iterations = test_async[1]
        slow_down = test_async[2]

        # Randomly shuffle the agent ids if a negative value was given for [% slow agents]
        agent_ids = list(collective_config.keys())
        if percent_slow_agents < 0.0:
            percent_slow_agents = -percent_slow_agents
            random.shuffle(agent_ids)
        is_slow_agent = {
            agent_id: (i + 1) / num_agents <= percent_slow_agents
            for i, agent_id in enumerate(agent_ids)
        }

    # Launch the agent subprocesses
    for agent_id, agent_info in collective_config.items():
        address = agent_info["address"]
        port = agent_info["port"]
        nbrs = agent_info["nbrs"]
        nbr_addresses = agent_info["nbr_addresses"]
        nbr_ports = agent_info["nbr_ports"]

        # Build the command line for the agent subprocess
        cmd = [sys.executable, "-u"]
        if profile:
            cmd += ["-m", "cProfile", "-s", "cumtime"]
        cmd += (
            [
                script,
                "--agent_id",
                str(agent_id),
                "--address",
                str(address),
                "--port",
                str(port),
                "--nbr_addresses",
            ]
            + [str(a) for a in nbr_addresses]
            + ["--nbr_ports"]
            + [str(p) for p in nbr_ports]
        )
        if test_async:
            cmd += [
                "--test_async",
                str(float(is_slow_agent[agent_id])),
                str(percent_slow_iterations),
                str(slow_down),
            ]
        if kwargs:
            cmd += ["--kwargs"] + [str(k) + "=" + str(v) for (k, v) in kwargs.items()]

        print(
            f"Launching agent {agent_id} at address:port {address}:{port} with neighbors {nbrs}"
        )

        # Start the agent subprocess with output capture
        proc = subprocess.Popen(
            cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1
        )
        processes.append(proc)

        if unbuffered:
            # Separate threads for stdout and stderr of this process
            t_out = threading.Thread(
                target=stream_output,
                args=(f"agent-{agent_id} stdout", proc.stdout),
                daemon=True,
            )
            t_err = threading.Thread(
                target=stream_output,
                args=(f"agent-{agent_id} stderr", proc.stderr),
                daemon=True,
            )
            t_out.start()
            t_err.start()
            threads.extend([t_out, t_err])

    returncodes = []
    if not unbuffered:
        # Wait for all agents to finish with timeout and output handling
        for proc in processes:
            try:
                outs, errs = proc.communicate(timeout=120)
                # WM: todo - I have seen cases where the returncode is 0 despite errors... has to do with
                #            threads launched by the job failing, but this error doesn't propagate up to
                #            the python subprocess here, which thinks it exited normally?
                if proc.returncode != 0 or print_to_screen:
                    print(f"Process exited with code {proc.returncode}.")
                    print("Stdout:", outs)
                    print("Stderr:", errs)
            except subprocess.TimeoutExpired:
                proc.kill()
                outs, errs = proc.communicate()
                print("Process timed out and was killed.")
                print("Stdout:", outs)
                print("Stderr:", errs)
            returncodes.append(proc.returncode)
    else:
        # Wait for all processes to finish
        for proc in processes:
            proc.wait()
            returncodes.append(proc.returncode)

        # Ensure all output has been read
        for t in threads:
            t.join()

    print("\nAll agents finished.")
    return returncodes


def main():
    # Parse command line arguments
    parser = argparse.ArgumentParser(
        prog="Skywing driver",
        description="Create and run a Skywing collective where each Skywing agent runs a generic python script. "
        "The collective may be defined either by a JSON config file (see ../tests/test_configs for examples) "
        "OR by the num_agents, starting_port, and comm_topology options (passing a config file will override "
        "these options). Additionally, artificial slow-downs may be applied to agents via the test_async option.",
        epilog="Example: python driver.py ../examples/simple_iteration.py --kwargs processor=Max --print",
    )
    parser.add_argument(
        "script",
        type=str,
        help="Path to script script to be run by the Skywing collective.",
    )
    add_driver_default_args(parser)
    args = parser.parse_args()
    kwargs = argparse_list_to_kwargs(args.kwargs)

    # Run the Skywing collective
    run_collective(
        args.script,
        config=args.config,
        num_agents=args.num_agents,
        starting_port=args.starting_port,
        comm_topology=args.comm_topology,
        print_to_screen=args.print,
        unbuffered=args.unbuffered,
        test_async=args.test_async,
        profile=args.profile,
        **kwargs,
    )


if __name__ == "__main__":
    main()
