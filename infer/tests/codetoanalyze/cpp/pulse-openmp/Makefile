# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

TESTS_DIR = ../../..

# -fopenmp is the point of this directory: without it the pragmas in the source are ignored and
# every case is ordinary code, so the flag is what makes this a test of OpenMP regions at all.
CLANG_OPTIONS = -x c++ -std=c++17 -fopenmp -nostdinc++ -isystem$(CLANG_INCLUDES)/c++/v1/ -c
# liveness is on as well as pulse: a dead store reported on a variable that a region reads, or
# that a region is the only writer of, is the other half of what dropping the body caused.
INFER_OPTIONS = --pulse-only --liveness --debug-exceptions --project-root $(TESTS_DIR)
INFERPRINT_OPTIONS = --issues-tests

SOURCES = $(wildcard *.cpp)

include $(TESTS_DIR)/clang.make
