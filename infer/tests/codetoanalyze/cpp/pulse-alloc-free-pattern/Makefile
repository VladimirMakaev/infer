# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

TESTS_DIR = ../../..

# see explanations in cpp/pulse/Makefile for the custom isystem
CLANG_OPTIONS = -x c++ -std=c++17 -nostdinc++ -isystem$(CLANG_INCLUDES)/c++/v1/ -c

# the point of this directory: the custom alloc/free patterns against a NAMESPACED callee.
# cpp/pulse covers the models that need no pattern, and c/pulse's .inferconfig covers the
# global C shape, which must keep working.
INFER_OPTIONS = --pulse-only --debug-exceptions --project-root $(TESTS_DIR) \
  --pulse-model-alloc-pattern '.*open_session.*' \
  --pulse-model-free-pattern '.*close.*' \
  --enable-issue-type MEMORY_LEAK_CPP

INFERPRINT_OPTIONS = --issues-tests

SOURCES = $(wildcard *.cpp)

include $(TESTS_DIR)/clang.make

infer-out/report.json: $(MAKEFILE_LIST)
