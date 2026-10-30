# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

TESTS_DIR = ../../..

# Re-capturing a file with --mark-unchanged-procs compares each procedure with the one already in
# the database and rewrites its row when they differ. A procedure too big to store must degrade
# on that path too, not abort the capture. The first pass stores the procedures under a limit
# large enough for all of them; the second re-captures them under one the big procedure cannot
# meet.

CLANG_OPTIONS = -c
INFER_OPTIONS = --pulse-only --debug-exceptions --project-root $(TESTS_DIR)
INFER_OPTIONS_STORE = --sqlite-max-blob-size 100000000
INFER_OPTIONS_RECAPTURE = --mark-unchanged-procs --sqlite-max-blob-size 100000

INFERPRINT_OPTIONS = --issues-tests

SOURCES = $(wildcard *.c)

include $(TESTS_DIR)/infer.make
include $(TESTS_DIR)/clang-base.make

$(INFER_OUT)/report.json: $(CLANG_DEPS) $(SOURCES) $(HEADERS) $(TESTS_DIR)/.inferconfig $(MAKEFILE_LIST)
	$(QUIET)$(call silent_on_success,Testing infer/clang in $(TEST_REL_DIR)$(TEST_SUB_TITLE),\
	  $(INFER_BIN) capture --results-dir $(@D) \
	    $(INFER_OPTIONS) $(INFER_OPTIONS_STORE) -- \
	    clang $(CLANG_OPTIONS) $(SOURCES) && \
	  $(INFER_BIN) --results-dir $(@D) --dump-duplicate-symbols \
	    $(INFER_OPTIONS) $(INFER_OPTIONS_RECAPTURE) -- \
	    clang $(CLANG_OPTIONS) $(SOURCES))
