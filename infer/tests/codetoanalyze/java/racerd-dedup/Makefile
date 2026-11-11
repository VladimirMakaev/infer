# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

TESTS_DIR = ../../..

INFER_OPTIONS = --racerd-only --racerd-guardedby --deduplicate
INFERPRINT_OPTIONS = \
  --issues-tests-fields file,procedure,line_offset,bug_type,qualifier,bug_trace,racerd_access_extra \
  --issues-tests
SOURCES = $(wildcard *.java)

include $(TESTS_DIR)/javac.make
