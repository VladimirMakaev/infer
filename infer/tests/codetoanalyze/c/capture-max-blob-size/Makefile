# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

TESTS_DIR = ../../..

# A procedure whose body is bigger than --sqlite-max-blob-size is captured without one, and the
# rest of the translation unit is still analysed

CLANG_OPTIONS = -c
INFER_OPTIONS = \
  --pulse-only --debug-exceptions --project-root $(TESTS_DIR) \
  --sqlite-max-blob-size 100000 \

INFERPRINT_OPTIONS = --issues-tests

SOURCES = $(wildcard *.c)

include $(TESTS_DIR)/clang.make
