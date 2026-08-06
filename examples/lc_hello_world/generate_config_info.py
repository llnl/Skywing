import sys


def read_args(args):
    def int_reader(x):
        return int(x)

    # float_reader is unused but we'll keep it as a defined function
    def float_reader(x):
        return float(x)

    def string_reader(x):
        return x

    def ensure_trailing_reader(tr):
        def inner(x):
            return x.rstrip(tr) + tr

        return inner

    def array_reader(element_reader):
        def inner(x):
            return [element_reader(z) for z in x.split(",")]

        return inner

    # Define reader functions for each parameter
    reader_fns = {
        "num_agents": int_reader,
        "config_filename": string_reader,
        "node_list": string_reader,
        "port_start": int_reader,
    }

    params_dict = dict()
    params_dict["port_start"] = 20000
    try:
        for a in args[1:]:
            tokens = a.split("=")
            params_dict[tokens[0]] = reader_fns[tokens[0]](tokens[1])
    except Exception as e:
        exit(
            str(e) + "\n\nCommand line format: python generate_linsys_data.py "
            "num_agents=(int) config_file=(dir) [port_start=(int)]"
        )
    return params_dict


def parse_node_list(node_list_string):
    if "[" not in node_list_string:
        assert "," not in node_list_string
        return [node_list_string]
    cluster_name, node_inds_list = node_list_string.split("[")
    node_inds_list = node_inds_list.split("]")[0].split(",")
    node_names_list = []
    for n in node_inds_list:
        if "-" not in n:
            node_names_list.append(cluster_name + str(n))
        else:
            # If it has a hyphen it defined a range
            bounds = n.split("-")
            node_names_list.extend(
                [
                    cluster_name + str(x)
                    for x in range(int(bounds[0]), int(bounds[1]) + 1)
                ]
            )
    return node_names_list


def generate_config_file(num_agents, node_names_list, filename, port_start):
    """Generate a file containing Skywing agent configuration data.

    Inputs:
    num_agents - (int) The number of agents in the Skywing network
    node_names_list - (list of strings) The names of the LC nodes in the job
    filename - (string) Where to write this file.
    port_start - (int) The port to begin counting at for each node.
    """
    f = open(filename, "w")
    num_agents_per_node = int(num_agents / len(node_names_list))
    port_num = port_start
    for i, name in enumerate(node_names_list):
        for j in range(num_agents_per_node):
            agent_num = num_agents_per_node * i + j
            f.write(f"agent{agent_num!s}\n")
            f.write(f"{name}.llnl.gov\n")
            f.write(str(port_num) + "\n")
            port_num += 1
            if agent_num < num_agents - 1:
                f.write(f"agent{agent_num + 1!s}\n")
            f.write("---\n")
    f.close()


##############
# Main funtion
##############

if __name__ == "__main__":
    params_dict = read_args(sys.argv)

    node_names_list = parse_node_list(params_dict["node_list"])
    generate_config_file(
        params_dict["num_agents"],
        node_names_list,
        params_dict["config_filename"],
        params_dict["port_start"],
    )
