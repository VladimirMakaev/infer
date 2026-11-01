# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

TESTS_DIR = ../../..

# see explanations in cpp/pulse/Makefile for the custom isystem
CLANG_OPTIONS = -x c++ -std=c++17 -nostdinc++ -isystem$(ROOT_DIR) -isystem$(CLANG_INCLUDES)/c++/v1/ -c

# the point of this directory: exercise --racerd-ignore-procedures. cpp/racerd pins the
# behaviour without it.
INFER_OPTIONS = --racerd-only --racerd-ignore-procedures 'ignore_procs::handleAssert' \
  --debug-exceptions --project-root $(TESTS_DIR)

INFERPRINT_OPTIONS = --issues-tests

SOURCES = $(wildcard *.cpp)

include $(TESTS_DIR)/clang.make

infer-out/report.json: $(MAKEFILE_LIST)
