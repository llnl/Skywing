# Testing setup

Tests for the core communication routines are in `test_python_core.py`.
Test for the example codes covering the algorithms implemented in Skywing are in `test_examples.py`.
Generally, testing the correctness of individual algorithms may be accomplished by running
the `simple_iteration.py` example script through the testing framework implemented here.
In addition to the standard command line arguments for setting up a Skywing collective, this script
accepts the additional keyword arguments, 
`--kwargs processor=<processor> data_type=<data_type> data_file=<data_file> output_file=<output_file>`,
that specify the processor to run, the type of data and data file the processor will ingest,
and an output file where results will be saved. The implementaiton in `test_examples.py` is
setup to automatically generate unique output files (saved under the `artifacts` directory)
that can be read and compared with expected results to test correctness.
Adding a new test may be done as follows. 

## Create a subclass of `ExampleTest`

Create a subclass of `ExampleTest` for your test, implementing the `expected_output` and `assert_output`
functions, which respectively define how to generate the expected output for your test and how to compare
the test result read from the generated output file with the expected results.
For example, the `MaxExample` below reads the input file (expected to be a csv file with scalar data
associated with each agent) and gets the max over the last row of the input data.
Then to assert correctness of the test, the result from the output file (stored in the `content`
variable below) is compared with the expected value. In this case, the numbers should match
to a small absolute tolerance.
```
class MaxExample(ExampleTest):
    def expected_output(self, agent_id):
        df = pd.read_csv(self.input_data_file)
        last_row_max = df.iloc[-1].max()
        return last_row_max

    def assert_output(self, content: str, expected) -> None:
        # Assert that the numerical values match within a small tolerance
        assert abs(float(content) - float(expected)) < 0.00001
```

## Add to the list of tests

Add instances of your new `ExampleTest` subclass to one of the existing test lists
according to which backends need to be tested. For example, the `MaxExample` is tested
with both the Python and C++ backends, so it is added to the `EXAMPLES_BOTH` list.
When instantiating the class object, pass all necessary keyword arguments such as
a name for the test (e.g. `name="max"`), the path to the executable to run (typically
`example_path="examples/simple_iteration.py"`), the path to the data file containing
the input (`data_file=<...>`), the data type if necessary (the default is `data_type="scalar"`),
the config file specifying the collective (where possible, you can use existing config
files, e.g. `config_file="tests/test_configs/3_line.json"`), the processor that
`simple_iteration.py` will run (e.g. `processor="Max"`), and finally any additional
kwargs required by the processor.

```
EXAMPLES_BOTH = [
    MaxExample(
        name="max",
        example_path="examples/simple_iteration.py",
        data_file="tests/test_data/scalar_test.csv",
        config_file="tests/test_configs/3_line.json",
        processor="Max",
    ),
```
