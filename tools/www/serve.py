"""Serves the built site on 127.0.0.1 only: bazel run //:serve [-- PORT].

The port is 8000 unless given. It runs `http.server` with the Python that rules_python downloads.
Ctrl-C stops it.
"""

import runpy
import sys

site, *rest = sys.argv[1:]
sys.argv = ["http.server", "--bind", "127.0.0.1", "--directory", site, *rest]
runpy.run_module("http.server", run_name="__main__", alter_sys=True)
