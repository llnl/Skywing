```
          _|                                  _|
  _|_|_|  _|  _|    _|    _|  _|          _|      _|_|_|      _|_|
_|_|      _|_|      _|    _|  _|    _|    _|  _|  _|    _|  _|    _|
    _|_|  _|  _|    _|    _|  _|  _|  _|  _|  _|  _|    _|  _|    _|
_|_|_|    _|    _|    _|_|_|    _|      _|    _|  _|    _|    _|_|_|
                          _|                                      _|
                      _|_|                                    _|_|
```

A high-reliability, real-time, decentralized platform for collaborative autonomy.

# Overview

Skywing is a Python library that provides tools to build decentralized, parallel, highly-asynchronous software that is resilient to hardware failures, network failures, and malicious actors. The library offers resilient software and algorithmic "building blocks" that implement functionality designed to adapt around problems.

## Core Architecture

Skywing's architecture is built around these key concepts:

1. **Agents**: Individual devices running Skywing software
2. **Collective**: A collection of Skywing agents working together
3. **Manager**: The core thread that handles agent-to-agent connections, search protocols, and messaging
4. **Job**: A user-defined thread of execution that declares publications, subscriptions, and operations

Skywing agents communicate via a publish/subscribe paradigm using unique string IDs called "tags". The architecture is designed to enable highly asynchronous workflows, where agents only need minimal knowledge about the collective at startup.

# Installation

## Requirements

* Python 3.10 or higher
* **Platform**: Linux or macOS (Windows is not supported)
* Dependencies (automatically installed):
  * numpy
  * matplotlib
  * networkx
  * scipy
  * pandas
  * loguru
  * pydantic
  * msgpack

## Install from Source

```bash
# Clone the repository
git clone <repository-url>
cd Skywing

# Create and activate a virtual environment (recommended)
python -m venv .venv
source .venv/bin/activate  # On Unix/Mac
# OR
.venv\Scripts\activate     # On Windows

# Install the package
pip install .

# For development (includes testing and linting tools)
pip install .[dev,test]
```

## Verify Installation

```bash
python -c "import skywing; print('Skywing installed successfully')"
```

# Quick Start

## Running Examples

Skywing includes a flexible example framework using `simple_iteration.py` that supports various distributed algorithms:

```bash
# Navigate to the Python examples directory
cd src/examples

# Example: Run max processor with 3 agents on a ring topology
# Use the driver to launch multiple agents automatically
cd ..  # Back to src/ directory
python -m skywing.drivers.driver examples/simple_iteration.py \
  --num_agents 3 \
  --comm_topology ring \
  --kwargs processor=Max num_calls=5 data_type=scalar

# Or manually launch agents in separate terminals:
# Terminal 1:
python examples/simple_iteration.py --port 20000 --nbr_ports 20001 20002 --kwargs processor=Max

# Terminal 2:
python examples/simple_iteration.py --port 20001 --nbr_ports 20000 20002 --kwargs processor=Max

# Terminal 3:
python examples/simple_iteration.py --port 20002 --nbr_ports 20000 20001 --kwargs processor=Max
```

The `simple_iteration.py` framework supports multiple processors via the `--kwargs processor=<name>` argument. See the Available Processors section below for options.

## Using the Driver

The driver system provides a convenient way to launch multiple agents:

```bash
# Run using the driver with a configuration
skywing_drive --config <config_file>
```

# Development

## Running Tests

```bash
# Run all tests
pytest src/tests/

# Run specific test file
pytest src/tests/test_examples.py

# Run with verbose output
pytest -v src/tests/
```

## Code Quality

This project uses [Ruff](https://docs.astral.sh/ruff/) for Python code linting and formatting, and [basedpyright](https://github.com/detachhead/basedpyright) for type checking.

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

See [LINTING.md](LINTING.md) for more details on code quality standards.

## Project Structure

```
Skywing/
├── src/
│   ├── skywing/          # Core library
│   │   ├── core/         # Agent, Manager, Job classes
│   │   ├── mid/          # Processors (SGD, Max, Sum, ADMM, COLA, etc.)
│   │   └── drivers/      # Driver utilities
│   ├── examples/         # Example implementations
│   └── tests/            # Test suite
├── docs/                 # Documentation and tutorials
├── pyproject.toml        # Package configuration
└── README.md             # This file
```

# Available Processors

Skywing includes several built-in processors for distributed algorithms:

- **Max/Min Processors**: Find maximum/minimum values
- **Sum Processor**: Compute distributed sum with push-sum algorithm
- **Simple Sum Processor**: Basic summation
- **Count Processor**: Count agents in collective
- **SGD Processor**: Stochastic gradient descent
- **ADMM Processor**: Alternating Direction Method of Multipliers
- **COLA Processor**: Coordinate-descent-based algorithm
- **Jacobi Processor**: Jacobi iterative method
- **Push Sum Processor**: Gossip-based averaging
- **SONATA Processor**: Distributed optimization

# Documentation

For more detailed documentation, see:
- `docs/` folder - main documentation 
- `CLAUDE.md` - Claude file

# Contributing to Skywing

Skywing is an open source project. We welcome contributions via pull requests as well as questions, feature requests, or bug reports via issues. Contact any of our team members with any questions. Please also refer to our [code of conduct](CODE_OF_CONDUCT.md).

## Contribution Guidelines

* Create your branches off the `main` branch
* Clearly name your branches, commits, and PRs
* Articulate your commit messages in the imperative (e.g., "Add new feature" not "Added new feature")
* Commit your work in logically organized commits
* Title each PR clearly and give it an unambiguous description
* Review existing issues before opening a new one
* Be explicit when opening issues and reporting bugs

## Code Quality Standards

All Python code must:
- Pass Ruff linting and formatting checks
- Pass basedpyright type checking
- Include appropriate tests
- Follow PEP 8 style guidelines

The GitLab CI pipeline includes checks that will fail if the code doesn't meet these standards.

# Current Development Team

* Tom Benson <benson31@llnl.gov>
* Annika Mauro <mauro3@llnl.gov>
* Wayne Mitchell <mitchell82@llnl.gov>
* Sarah Osborn <osborn9@llnl.gov>
* Colin Ponce <ponce11@llnl.gov>
* Alyson Fox <fox33@llnl.gov>

# Previous Members

* Michael Brzustowicz <brzustowicz1@llnl.gov>
* Kendall Harter <harter8@llnl.gov>
* Rachel Waldon <waldon1@llnl.gov>

# License

Skywing is distributed here under the GPL v2.0 license, but a commercial license is also available. Users may choose either license, depending on their needs.

For the commercial license, please inquire at <softwarelicensing@lists.llnl.gov>.

LLNL-CODE-835832
