# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

TESTS_DIR = ../../..

# see explanations in cpp/pulse/Makefile for the custom isystem
CLANG_OPTIONS = -x c++ -std=c++17 -nostdinc++ -isystem$(CLANG_INCLUDES)/c++/v1/ -c
# The point of this directory: a listed callee that returns a SMART pointer. It needs list entries
# of its own for the unique_ptr and raw-pointer shapes, which is why it is a separate case rather
# than another file in cpp/pulse-failure-outcomes -- that suite covers the raw-pointer sources and
# the fatal sinks, and its list names only the shared_ptr getter.
# PULSE_FATAL_ON_NULLABLE_RETURN is registered disabled, so it has to be turned on here.
INFER_OPTIONS = \
  --pulse-only --debug-exceptions --no-pulse-force-continue --project-root $(TESTS_DIR) \
  --pulse-report-issues-for-tests \
  --enable-issue-type PULSE_FATAL_ON_NULLABLE_RETURN \
  --pulse-model-return-nullable-list 'mr::VulkanContextProvider::rhi$$' \
  --pulse-model-return-nullable-list 'mr::VulkanContextProvider::makeRhi$$' \
  --pulse-model-return-nullable-list 'mr::VulkanContextProvider::rawRhi$$'
INFERPRINT_OPTIONS = \
  --issues-tests-fields file,procedure,line_offset,bug_type,bucket,severity,bug_trace,taint_extra,transitive_callees_extra,autofix \
  --issues-tests

SOURCES = $(wildcard *.cpp)

include $(TESTS_DIR)/clang.make

infer-out/report.json: $(MAKEFILE_LIST)
