@defgroup sync-jacobi Synchronizing Jacobi 

# Skywing Debug mode - Synchronizing root node

# Overview

Debug mode Synchronizing root node allows us to run an `AsynchronousIterative` method in a synchronous way by inserting barrier within each iteration.
In order to implement a barrier, we add a "root node" which runs alongside the collective and can signal the collective to continue iterating.
We also add a `SynchronizingRootNode` iteration policy which stops the agents in each iteration until they receive the signal.
In order to use the debug mode, we must run both a root node and ensure all agents are using this iteration policy. 

# How to run debug mode with the jacobi example

## To run locally

- Edit `config.cfg` to reflect the number of agents you want in your collective. For example, a 2 agent collective running locally could have a `config.cfg` file like:

```
agent0
localhost
30000
agent1
---
agent1
localhost
30001
---
```

- Open a terminal window for each agent in the config file. In each window, run
```
./synchronizing_jacobi config.cfg <agent index>
```
You should see them establish their connections and then wait.

- In another window, run 
```
./root_node config.cfg
```
You should then see the whole collective begin iterating until timeout. To configure the timeouts, edit `root_node.cpp ` and `synchronizing_jacobi.cpp`.

# How to add debug mode to a new example

The root node is provided with the global configuration file for the collective, which includes the ip addresses and ports of each of the agents. 

The root node uses the Job ID to label the agents, so we MUST use *unique* Job IDs for the root node to be able to tell the agents apart. 

To run an `AsynchronousIterative` method in debug mode: 
1. Change the iteration policy to `SynchronizingRootNode`
2. Within the root node script, `root_node.cpp`, subscribe to the Job ID as the agent tags. Note: In a future version of debug mode, the root node could query the agents to get their job ids. In the meantime, we must provide the job IDs we use to spawn the jobs (this is the first argument to `manager.submit_job()`).
3. Run the root node alongside the collective.

# Important considerations

Both the root node and the synchronizing root node iteration policy accept a timeout duration in seconds as an argument. Whichever timeout duration is shorter will become the timeout duration of the collective, as both the root node and the agents need to be running for iteration to occur. Be mindful that the timing begins when an agent is started, regardless of whether the collective is iterating.

The tags `"root_signal"` and the job ID are used as tags, and so MUST NOT be used as tags elsewhere in the skywing collective. Using the job ID or the string `"root_signal"` as a tag will lead to unwanted behaviour. 

Running multiple jobs with a root node has not been tested - current support is for collectives with each agent running one `AsynchronousIterative` method job. 
