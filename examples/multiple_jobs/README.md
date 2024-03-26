```
           _|                                  _|
   _|_|_|  _|  _|    _|    _|  _|          _|      _|_|_|      _|_|
 _|_|      _|_|      _|    _|  _|    _|    _|  _|  _|    _|  _|    _|
     _|_|  _|  _|    _|    _|  _|  _|  _|  _|  _|  _|    _|  _|    _|
 _|_|_|    _|    _|    _|_|_|    _|      _|    _|  _|    _|    _|_|_|
                           _|                                      _|
                       _|_|                                    _|_|
```

# Multiple Jobs

This is example showcases agents running multiple jobs
concurrently in a structure that is likely common in application
usage. Often, Skywing collectives need to perform two tasks:

1. Use incoming data to perform some computation, and continually
update that computation as new data come in.

2. Use the result of that computation to make some decision or perform
another computation.

A clunky approach to this would be to have each agent iterate back and
forth between Task 1 and Task 2. However, doing this successfully
would require coordination between the agents, and would require
agents somehow decide when to switch between Tasks. This is, at best,
difficult to do reliably.

A better approach is to have each agent continually doing _both_ tasks. In this example, each agent runs two jobs:

- *Summation Job:* Continually performs a collective summation. Each
   agent contributes a value to the summation; this job subscribes to
   a data stream through which it receives this agent's contribution.

- *Input/Output Job:* Supply inputs to, and use outputs from, the
  collective summation. This job publishes a data stream representing
  updated contribution values to the summation. It also subscribes to
  a data stream representing the output values from the summation.

In some applications, the *Input/Output Job* may be two separates jobs.

## A note on publications.

Note that contribution update streams are specific to an agent, while
summation output streams are not. This can be seen in the tags. On
Agent X, the contribution update tag has an ID that is specific to
Agent X; that is, Agent 0 publishes "contribution_update0" while Agent
1 publishes "contribution_update1". On the other hand, the summation
output tag simply has ID "summation_result", a tag that is not
specific to this agent.

What this means is that _every agent publishes the summation output
under the same tag_ (See Tutorial 02 for discussion of this). This is
not a problem, and is often of benefit. When a job subscribes to a
data stream, the agent will seek _some_ agent that publishes under the
tag; it can be itself or it can be another agent. The fact that many
options exist provides resilience benefits.

## Running the example.

To run the example, source the run script with a starting port:
```
source run.sh [start_port]
```
For example,
```
source run.sh 20000
```

## What to see in the output.

The output will take a couple seconds to really start. Then, a
continual stream of updates from the summation jobs will be
produced. Every 3 seconds, the input/output jobs will read off the
summation value from their subscriptions and print it.

Every 12 seconds, the input/output jobs will update the contribution
value publication, increasing it by a factor of 10. As a result, the
summation job will react, noisily adjusting to the new values before
converging to the new sum.