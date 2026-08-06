# Python Code Linting and Formatting

This project uses [Ruff](https://docs.astral.sh/ruff/) for Python code linting and formatting, and [basedpyright](https://github.com/detachhead/basedpyright) for type checking.

## CI Checks

The GitLab CI pipeline includes automatic checks for Python code quality:

- `ruff check`: Verifies that code follows the defined linting rules
- `ruff format`: Checks that code follows the defined formatting style
- `basedpyright`: Performs static type checking to catch type-related errors

These checks run in the CI pipeline, and PRs will fail if the code doesn't meet the standards.

## Running Linting Checks Locally

To check and fix linting and formatting issues locally before pushing:

1. Install development dependencies:

```bash
pip install -e .[dev]
# Or directly install the tools
pip install ruff>=0.3.0 basedpyright
```

2. Check for linting issues:

```bash
# Check for issues
ruff check python/

# Fix auto-fixable issues
ruff check --fix python/
```

3. Check and apply formatting:

```bash
# Check formatting only
ruff format --check python/

# Apply formatting
ruff format python/
```

4. Run type checking:

```bash
# Run type checking using basedpyright
basedpyright
```

## Configuration

Linting and formatting rules are defined in the `pyproject.toml` file:

- Ruff configuration is under the `[tool.ruff]` section
- basedpyright configuration is under the `[tool.basedpyright]` section

## Common Issues

If your CI pipeline fails due to linting or formatting issues:

1. Pull the latest code
2. Run `ruff check --fix python/` to fix linting issues
3. Run `ruff format python/` to fix formatting issues
4. Fix type errors identified by `basedpyright`
5. Commit and push the changes

For more information:
- [Ruff documentation](https://docs.astral.sh/ruff/rules/)
- [Python Type Hints documentation](https://docs.python.org/3/library/typing.html)
- [basedpyright GitHub](https://github.com/detachhead/basedpyright)