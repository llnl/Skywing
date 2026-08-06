# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

import os
import sys

# Add the python directory to the path so Sphinx can find the modules
sys.path.insert(0, os.path.abspath("python"))

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = "Skywing"
copyright = "2026, "
author = "Annika Mauro, Wayne Mitchell, Shayna Kapadia, Tom Benson, Colin Ponce, Sarah Osborn, Alyson Fox"
release = "0.2"

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = [
    "sphinx.ext.autodoc",  # Automatically document from docstrings
    "sphinx.ext.napoleon",  # Support for NumPy and Google style docstrings
    "sphinx.ext.viewcode",  # Add links to highlighted source code
    "sphinx.ext.intersphinx",  # Link to other project's documentation
    "sphinx.ext.autosummary",  # Generate autodoc summaries
]

# Autodoc settings
autodoc_default_options = {
    "members": True,
    "member-order": "bysource",
    "special-members": "__init__",
    "undoc-members": True,
    "exclude-members": "__weakref__",
}

# Prevent duplicate documentation of inherited members
autodoc_inherit_docstrings = True

# Autosummary settings
# Disable autosummary generation since we use manual documentation
autosummary_generate = False

# Napoleon settings for Google/NumPy style docstrings
napoleon_google_docstring = True
napoleon_numpy_docstring = True
napoleon_include_init_with_doc = True
napoleon_use_ivar = (
    False  # Don't document instance variables separately from docstring attributes
)
napoleon_use_param = True
napoleon_use_rtype = True
napoleon_preprocess_types = False

# Intersphinx mapping
intersphinx_mapping = {
    "python": ("https://docs.python.org/3", None),
    "numpy": ("https://numpy.org/doc/stable/", None),
}

templates_path = ["_templates"]
exclude_patterns = [
    "_build",
    "Thumbs.db",
    ".DS_Store",
    "skbuild",
    "build",
    ".venv",
    "skywing_pyenv",
    "**/site-packages/**",
]

# Suppress duplicate object warnings
suppress_warnings = [
    "autodoc.duplicate_object",
    "ref.python",  # Suppress warnings about duplicate Python object descriptions
]

# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = "pydata_sphinx_theme"
html_static_path = ["_static"]

# HTML title (appears in browser tab and page header)
html_title = "Skywing"

# Configure sidebars per page (no left sidebar on these pages)
# Note: Empty list removes all sidebars including the TOC on the right
# html_sidebars = {
#     'docs/install': [],
#     'docs/package_overview': [],
# }

# Theme options
html_theme_options = {
    "navigation_depth": 4,
    "collapse_navigation": False,
    "navbar_end": ["navbar-icon-links", "theme-switcher"],
    "navbar_center": ["navbar-nav"],
    "navigation_with_keys": False,
    # External links in the navigation bar
    "external_links": [],
    # Header links
    "header_links_before_dropdown": 5,
    # Primary navigation in the header
    "primary_sidebar_end": [],
    "show_nav_level": 2,  # Show 2 levels of navigation
    "show_toc_level": 2,  # Show 2 levels in the table of contents
}

# Master doc (root document)
master_doc = "index"
