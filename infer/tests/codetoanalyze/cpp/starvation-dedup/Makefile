# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

TESTS_DIR = ../../..

ANALYZER = checkers
# see explanations in cpp/pulse/Makefile for the custom isystem
CLANG_OPTIONS = -x c++ -std=c++11 -nostdinc++ -isystem$(ROOT_DIR) -isystem$(CLANG_INCLUDES)/c++/v1/ -c

INFER_OPTIONS = --starvation-only --project-root $(TESTS_DIR)

# the qualifier is recorded because what this directory tests is which reports
# survive deduplication AND what the surviving one says about the locks it covers
INFERPRINT_OPTIONS = \
  --issues-tests-fields file,procedure,line_offset,bug_type,bucket,severity,bug_trace,qualifier \
  --issues-tests

SOURCES = $(wildcard *.cpp)

include $(TESTS_DIR)/clang.make

infer-out/report.json: $(MAKEFILE_LIST)
