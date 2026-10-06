# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

TESTS_DIR = ../../..

CLANG_OPTIONS = -x c++ -std=c++14 -nostdinc++ -isystem$(CLANG_INCLUDES)/c++/v1/ -c
# single class names rather than JSON lists
INFER_OPTIONS = --liveness-only --debug-exceptions --project-root $(TESTS_DIR) \
  --cxx-scope-guards liveness_options::Guard \
  --liveness-dangerous-classes liveness_options::Dangerous
INFERPRINT_OPTIONS = --issues-tests

SOURCES = $(wildcard *.cpp)

include $(TESTS_DIR)/clang.make

infer-out/report.json: $(MAKEFILE_LIST)
