Installation
============

Requirements
------------

- Python 3.8 or higher

Basic Installation
------------------

To install Skywing from the project directory:

.. code-block:: bash

   pip install .

Development Installation
------------------------

To install Skywing from the project directory:

.. code-block:: bash

   pip install .[dev,test]

Dependencies
------------

Skywing automatically installs the following Python dependencies:

- numpy
- matplotlib
- networkx
- scipy
- pandas
- sphinx
- loguru
- pydantic

Additional dependencies for development and testing:

- **test**: pytest
- **dev**: ruff, basedpyright

Configuring Logging
-------------------

Skywing uses `loguru <https://github.com/Delgan/loguru>`_ for logging. To control log verbosity, you can set the ``LOGURU_LEVEL`` environment variable (e.g., ``DEBUG``, ``INFO``, ``WARNING``) or configure it programmatically in your script. See the `loguru documentation on levels <https://loguru.readthedocs.io/en/stable/api/logger.html#levels>`_ for details.

For debugging, we recommend ``LOGURU_LEVEL=DEBUG``. For production, we recommend ``LOGURU_LEVEL=WARNING`` or higher.