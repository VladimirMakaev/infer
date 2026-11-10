# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

TESTS_DIR = ../../..
CLANG_OPTIONS = -x c++ -std=c++17 -nostdinc++ -c
INFER_OPTIONS = \
  --pulse-only --debug-exceptions --no-pulse-force-continue --project-root $(TESTS_DIR)
INFERPRINT_OPTIONS = --issues-tests
SOURCES = $(wildcard *.cpp)
HEADERS = standin.h

include $(TESTS_DIR)/clang.make

infer-out/report.json: $(MAKEFILE_LIST)
