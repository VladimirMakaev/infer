# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

TESTS_DIR = ../../..

# use our own clang's standard library so that the tests are uniform across distributions
CLANG_OPTIONS = -x c++ -std=c++17 -nostdinc++ -isystem$(CLANG_INCLUDES)/c++/v1/ -c
INFER_OPTIONS = \
  --pulse-only --debug-exceptions --project-root $(TESTS_DIR) \
  --pulse-report-issues-for-tests --no-pulse-force-continue \
  --pulse-model-return-nullable dangerous --pulse-nullability-annotations \
  --pulse-skip-procedures skip_procedures:: \
  --report-suppress-errors SENSITIVE_DATA_FLOW --report-suppress-errors DATA_FLOW_TO_SINK
INFERPRINT_OPTIONS = \
  --issues-tests-fields file,procedure,line_offset,bug_type,bucket,severity,bug_trace,taint_extra,transitive_callees_extra,autofix \
  --issues-tests


SOURCES = $(wildcard *.cpp taint/*.cpp)
# clang.make makes the report depend on $(HEADERS), and this suite never set it, so editing a
# header left the expectations stale. It matters now that override_header.h carries a base class
# whose declaration decides what the override cells expect.
HEADERS = $(wildcard *.h)

include $(TESTS_DIR)/clang.make

infer-out/report.json: $(MAKEFILE_LIST)
