# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

# A capture whose process did not finish holds procedures but never recorded the file they
# belong to. Merging it with a good capture used to make the analysis of the good file die with
# "could not find tenv" as soon as it reached one of those procedures, which cost every file in
# the merge its results. The merge now leaves such a capture out and the good file is reported.

TESTS_DIR = ../..

CLANG_OPTIONS = -x c++ -std=c++17 -c
INFER_OPTIONS = --pulse-only --project-root $(PROJECT_ROOT)
INFERPRINT_OPTIONS = --issues-tests

SOURCES = good.cpp broken.cpp
CLEAN_EXTRA = infer-out-good infer-out-broken

include $(TESTS_DIR)/infer.make

PROJECT_ROOT ?= $(TESTS_DIR)

infer-out$(TEST_SUFFIX)/report.json: $(CLANG_DEPS) $(SOURCES) $(MAKEFILE_LIST) unrecord_source_file.py
	$(QUIET)$(call silent_on_success,Capturing good.cpp,\
	  $(INFER_BIN) capture --project-root $(PROJECT_ROOT) -o infer-out-good -- \
	    clang $(CLANG_OPTIONS) good.cpp -o /dev/null)
	$(QUIET)$(call silent_on_success,Capturing broken.cpp,\
	  $(INFER_BIN) capture --project-root $(PROJECT_ROOT) -o infer-out-broken -- \
	    clang $(CLANG_OPTIONS) broken.cpp -o /dev/null)
#	the procedures of broken.cpp are stored, the row naming the file they belong to is not: the
#	state a capture whose process is killed partway leaves behind
	$(QUIET)$(call silent_on_success,Unrecording the source file of the unfinished capture,\
	  python3 ./unrecord_source_file.py infer-out-broken/capture.db)
	$(QUIET)$(call silent_on_success,Merging the good and the unfinished capture,\
	  $(INFER_BIN) capture --project-root $(PROJECT_ROOT) -o $(@D) \
	    --merge-capture $$(realpath infer-out-good) --merge-capture $$(realpath infer-out-broken))
	$(QUIET)$(call silent_on_success,Testing infer/clang in $(TEST_REL_DIR),\
	  $(INFER_BIN) analyze --results-dir $(@D) $(INFER_OPTIONS))
