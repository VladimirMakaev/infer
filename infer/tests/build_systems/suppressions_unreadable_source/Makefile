# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

TESTS_DIR = ../..

include $(TESTS_DIR)/base.make

CLANG_OPTIONS = -c
INFERPRINT_OPTIONS = --issues-tests-fields "bug_type,file,procedure,suppressed" --issues-tests

SOURCES = src/a.c

default: test

# capture a copy of the sources and delete it before the analysis writes the report
issues.exp.test: $(CLANG_DEPS) $(SOURCES) $(MAKEFILE_LIST)
	$(QUIET)$(REMOVE_DIR) copy && $(MKDIR_P) copy && cp $(SOURCES) copy/ && \
	$(call silent_on_success,Testing suppressions in source files unreadable at report time,\
	  $(INFER_BIN) capture -- clang $(CLANG_OPTIONS) copy/a.c && $(REMOVE_DIR) copy && \
	  $(INFER_BIN) analyze 2> analyze.stderr && \
	  $(INFER_BIN) report -q $(INFERPRINT_OPTIONS) $@ && \
	  grep "suppressions were not applied" analyze.stderr >> $@)

.PHONY: print
print: issues.exp.test

.PHONY: test
test: issues.exp.test
	$(QUIET)cd $(TESTS_DIR) && \
	$(call check_no_diff,$(TEST_REL_DIR)/issues.exp,$(TEST_REL_DIR)/issues.exp.test)

.PHONY: replace
replace: issues.exp.test
	cp $< issues.exp

.PHONY: clean
clean:
	rm -rf analyze.stderr copy issues.exp.test infer-out
