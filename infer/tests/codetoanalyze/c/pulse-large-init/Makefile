# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

TESTS_DIR = ../../..

# What an initializer list too long to translate element by element means to an analysis: the
# elements are unknown, not zero, and the storage still counts as initialized. Options are kept
# to the minimum so that a missing report means the analysis did not find the issue.

CLANG_OPTIONS = -c
INFER_OPTIONS = --pulse-only --debug-exceptions --project-root $(TESTS_DIR)

INFERPRINT_OPTIONS = --issues-tests

SOURCES = $(wildcard *.c)

include $(TESTS_DIR)/clang.make
