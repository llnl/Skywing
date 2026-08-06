# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

Skywing is a high-reliability, real-time, decentralized platform for collaborative autonomy. It provides tools to build decentralized, parallel, highly-asynchronous software that is resilient to hardware failures, network failures, and malicious actors. The library offers resilient software and algorithmic "building blocks" that implement functionality designed to adapt around problems.

## Core Architecture

Skywing's architecture is built around these key concepts:

1. **Agents**: Individual devices running Skywing software
2. **Collective**: A collection of Skywing agents working together
3. **Manager**: The core thread that handles agent-to-agent connections, search protocols, and messaging
4. **Job**: A user-defined thread of execution that declares publications, subscriptions, and operations

Skywing agents communicate via a publish/subscribe paradigm using unique string IDs called "tags". The architecture is designed to enable highly asynchronous workflows, where agents only need minimal knowledge about the collective at startup.

## Directory Structure

- `/src/`: Python source code
  - `/skywing/core/`: Core Agent and Iteration classes
  - `/skywing/mid/`: Processors for different algorithms (SGD, COLA, etc.)
  - `/skywing/drivers/`: Driver utilities for launching agents
  - `/examples/`: Example implementations (max, sum, sgd, etc.)
  - `/tests/`: Python test suite
- `/docs/`: Tutorials and documentation

## Build and Install Commands

### Python Installation

```bash
# Create and activate a virtual environment (recommended)
python -m venv .venv
source .venv/bin/activate  # On Unix/Mac
# OR
.venv\Scripts\activate     # On Windows

# Install from the Skywing folder
pip install .

# For development with testing and linting tools
pip install .[dev,test]
```

## Common Development Tasks

### Running Tests

```bash
# Run all Python tests
pytest src/tests

# Run specific test file
pytest src/tests/test_examples.py

# Run with verbose output
pytest -v src/tests/
```

### Running Examples

```bash
# Navigate to the Python examples directory
cd src/examples

# Use the simple_iteration framework with different processors
python simple_iteration.py --port 20000 --nbr-ports 20001,20002 --kwargs processor=Max

# Run with SGD processor and data file
python simple_iteration.py --port 20000 --nbr-ports 20001,20002 --kwargs processor=SGD data_file=path/to/data.csv

# Or use the driver to launch multiple agents
cd ..  # Back to src/ directory
python -m skywing.drivers.driver --num_agents 3 --comm_topology ring --kwargs processor=Sum
```

## Key Components

### Python Components

- **Agent**: Defines a local address and neighboring connections for communication
- **Manager**: Handles TCP connections, message routing, and pub/sub
- **Job**: Base class for background operations
- **Iteration**: Defines an iterative algorithm that uses an Agent for communication
- **Processor**: Base class for implementing distributed algorithms (SGD, Max, etc.)

## Common Patterns and Best Practices

1. **Agent Setup**: Initialize agents with IP/port and configure neighbors
2. **Publish/Subscribe**: Use tags to publish and subscribe to data streams
3. **Iteration Pattern**: Create an iteration with a processor, launch it, and query for results
4. **Resilience**: Design algorithms to handle node failures and network issues

## Code Quality and Style

This project uses [Ruff](https://docs.astral.sh/ruff/) for Python code linting and formatting, and [basedpyright](https://github.com/detachhead/basedpyright) for type checking.

### Setting Up Code Quality Tools

```bash
# Install development tools
pip install -e .[dev]
```

### Running Code Quality Checks

```bash
# Run Ruff linter to check for issues
ruff check src/

# Run Ruff linter and fix issues automatically
ruff check --fix src/

# Run Ruff formatter to check formatting
ruff format --check src/

# Run Ruff formatter and apply formatting
ruff format src/

# Run basedpyright
basedpyright
```

### Code Quality Standards

1. All Python code must follow PEP 8 style guidelines
2. All code must pass Ruff linting and formatting checks
3. All code must pass basedpyright type checking
4. Fix all warnings and errors before committing

## Important Dependencies

- Python 3.10 or higher
- numpy
- matplotlib
- networkx
- scipy
- pandas
- loguru - for logging
- pydantic - for data validation
- msgpack - for serialization
- pytest - for testing
- ruff - for linting and formatting
- basedpyright - for type checking
