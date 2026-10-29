# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

TESTS_DIR = ../../..

# an optimised build that removes the macro itself: capture restores the level's macros, but a
# macro the command already names is the command's to decide, not ours.
CLANG_OPTIONS = -x c++ -std=c++17 -O2 -U__OPTIMIZE__ -nostdinc++ -isystem$(CLANG_INCLUDES)/c++/v1/ -c
INFER_OPTIONS = --pulse-only --debug-exceptions --project-root $(TESTS_DIR)
INFERPRINT_OPTIONS = --issues-tests

SOURCES = $(wildcard *.cpp)

include $(TESTS_DIR)/clang.make
