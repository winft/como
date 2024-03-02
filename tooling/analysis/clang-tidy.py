#!/usr/bin/env python

# SPDX-FileCopyrightText: 2024 Roman Gilg <subdiff@gmail.com>
# SPDX-License-Identifier: MIT

import argparse, os, subprocess, tempfile

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
SOURCE_DIR = os.path.dirname(os.path.dirname(SCRIPT_DIR))
RUN_SCRIPT_URL = "https://raw.githubusercontent.com/llvm/llvm-project/release/16.x/clang-tools-extra/clang-tidy/tool/run-clang-tidy.py"

def parse_arguments():
    parser = argparse.ArgumentParser(
        description="Run clang-tidy script with additional options"
    )
    parser.add_argument(
        "-p",
        dest="build_path",
        required=True,
        help="Path used to read a compile command database.",
    )
    parser.add_argument(
        "-j",
        type=int,
        default=0,
        help="number of tidy instances to be run in parallel.",
    )
    return parser.parse_args()

args = parse_arguments()

os.chdir(SOURCE_DIR)

# Download the run-clang-tidy.py script and save it as a temporary file
with tempfile.NamedTemporaryFile(mode="w") as temp_file:
    curl_process = subprocess.Popen(["curl", "-s", RUN_SCRIPT_URL], stdout=temp_file)
    curl_process.wait()
    run_clang_tidy_script_path = temp_file.name

    # Additional arguments for run-clang-tidy.py
    additional_args = [
        "-use-color",
        "-j", str(args.j),
        "-p=" + args.build_path,
        "-header-filter=.*",
        SOURCE_DIR,
    ]

    # Execute the modified script using Python with additional arguments
    python_process = subprocess.Popen(
        ["python", run_clang_tidy_script_path] + additional_args, stdin=subprocess.PIPE
    )
    python_process.communicate()
