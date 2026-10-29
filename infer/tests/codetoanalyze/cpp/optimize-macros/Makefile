# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

TESTS_DIR = ../../..

# -O2 is the point of this directory: capture must keep the macros the level defines, so a source
# that asserts it compiles, and one that branches on it is analysed in the form that is built.
CLANG_OPTIONS = -x c++ -std=c++17 -O2 -nostdinc++ -isystem$(CLANG_INCLUDES)/c++/v1/ -c
INFER_OPTIONS = --pulse-only --debug-exceptions --project-root $(TESTS_DIR)
INFERPRINT_OPTIONS = --issues-tests

SOURCES = $(wildcard *.cpp)

include $(TESTS_DIR)/clang.make
