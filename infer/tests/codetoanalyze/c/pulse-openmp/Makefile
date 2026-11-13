# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

TESTS_DIR = ../../..

# -fopenmp is the point of this directory: without it the pragmas below are ignored and every
# case is ordinary C, so the flag is what makes this a test of OpenMP regions at all. The C
# cases exist because the frontend path is shared but the suite flags are not, and a regression
# that only showed in C would otherwise be invisible.
CLANG_OPTIONS = -fopenmp -c
INFER_OPTIONS = --pulse-only --liveness --debug-exceptions --project-root $(TESTS_DIR)
INFERPRINT_OPTIONS = --issues-tests

SOURCES = $(wildcard *.c)

include $(TESTS_DIR)/clang.make
