# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

TESTS_DIR = ../../..

# see explanations in cpp/pulse/Makefile for the custom isystem
CLANG_OPTIONS = -x c++ -std=c++17 -nostdinc++ -isystem$(CLANG_INCLUDES)/c++/v1/ -c
# no --debug-exceptions here: it implies --no-filtering, which turns off the
# de-duplication this test is about
INFER_OPTIONS = \
  --pulse-only --no-pulse-force-continue --project-root $(TESTS_DIR)
INFERPRINT_OPTIONS = \
  --issues-tests-fields file,procedure,line_offset,bug_type,bucket,severity \
  --issues-tests

SOURCES = $(wildcard *.cpp)

include $(TESTS_DIR)/clang.make

infer-out$(TEST_SUFFIX)/report.json: .inferconfig $(MAKEFILE_LIST)
