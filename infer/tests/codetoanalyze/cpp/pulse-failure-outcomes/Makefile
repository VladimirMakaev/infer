# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

TESTS_DIR = ../../..

# see explanations in cpp/pulse/Makefile for the custom isystem
CLANG_OPTIONS = -x c++ -std=c++17 -nostdinc++ -isystem$(CLANG_INCLUDES)/c++/v1/ -c
# The two model options are existing flags, supplied here the way the generated nullable list and
# the real analyze config supply them, so this suite adds no new user-facing surface.
INFER_OPTIONS = \
  --pulse-only --debug-exceptions --no-pulse-force-continue --project-root $(TESTS_DIR) \
  --pulse-report-issues-for-tests \
  --pulse-model-return-nullable-list 'mr::VulkanContextProvider::rhi$$' \
  --pulse-model-return-nullable-list 'DesktopView::GetPanelAppHost$$' \
  --pulse-model-abort 'assertions::AssertHandler::handleAssert'
INFERPRINT_OPTIONS = \
  --issues-tests-fields file,procedure,line_offset,bug_type,bucket,severity,bug_trace,taint_extra,transitive_callees_extra,autofix \
  --issues-tests

SOURCES = $(wildcard *.cpp)

include $(TESTS_DIR)/clang.make

infer-out/report.json: $(MAKEFILE_LIST)
