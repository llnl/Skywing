Comparison to Other Tools
=========================

Skywing is designed for decentralized collaborative autonomy with an emphasis on fault tolerance. Here's how it relates to other distributed computing frameworks.


MPI (Message Passing Interface)
--------------------------------

**MPI** is the standard for high-performance computing and scientific simulations. It provides low-level, explicit message passing with fixed process topologies, designed for tightly-coupled parallel algorithms running on reliable HPC hardware. While MPI offers non-blocking communication primitives (MPI_Isend, MPI_Irecv), these still require explicit polling or waiting for completion rather than truly asynchronous execution. MPI requires all processes to be known at launch and typically fails completely when any process crashes, making it unsuitable for dynamic or fault-prone environments.


Ray
---

**Ray** is built for distributed machine learning and reinforcement learning applications. It uses a centralized Global Control Store (GCS) for metadata and scheduling, though actors can communicate directly via object references for data transfer. The focus is on ML training, hyperparameter tuning, and model serving in environments with reliable infrastructure. While actors have some peer-to-peer communication capabilities, the centralized control plane for scheduling and metadata limits its ability to operate in truly decentralized or unreliable network conditions.

Dask
----

**Dask** is designed for large-scale data analytics and parallel processing of dataframes. It uses a centralized scheduler to coordinate tasks across workers, making it well-suited for batch data processing pipelines and embarrassingly parallel computations. The centralized scheduler becomes a single point of failure, making it unsuitable for scenarios requiring decentralized resilience.

ROS (Robot Operating System)
-----------------------------

**ROS** is middleware for building robot software with perception, planning, and control tools. It's designed primarily for single-robot autonomy with local communication, providing a rich ecosystem of robotics-specific libraries and tools. While ROS 2 has improved multi-robot support, it's still primarily focused on local networks rather than wide-area decentralized coordination.


Skywing's Focus
---------------

Skywing is designed for scenarios requiring:

* Fully decentralized coordination with no central control
* Resilience to changing participation and device disconnections
* Unreliable network communication
* Streaming, potentially lossy data
* Heterogeneous device capabilities
