# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

TESTS_DIR = ../..

include $(TESTS_DIR)/base.make

CODETOANALYZE_DIR = ../codetoanalyze

CLANG_OPTIONS = -c
SOURCES = $(CODETOANALYZE_DIR)/hello.c
CLEAN_EXTRA = $(SOURCES:$(CODETOANALYZE_DIR)/%.c=%.o)

default: test

# .inferconfig gives a regex with PCRE-only syntax: Infer warns, and fails in strict mode
issues.exp.test: $(CLANG_DEPS) $(SOURCES) .inferconfig
	$(QUIET)$(call silent_on_success,Testing Infer warns about unsupported regex syntax,\
	  exit_code=0; INFER_STRICT_MODE=0 $(INFER_BIN) -- clang $(CLANG_OPTIONS) $(SOURCES) \
	    2> $@.stderr || exit_code=$$?; \
	  echo "infer exit code: $$exit_code" > $@; \
	  grep "regular expression" $@.stderr >> $@; \
	  exit_code=0; INFER_STRICT_MODE=1 $(INFER_BIN) -- clang $(CLANG_OPTIONS) $(SOURCES) \
	    2> $@.stderr || exit_code=$$?; \
	  echo "infer exit code in strict mode: $$exit_code" >> $@)

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
	rm -rf issues.exp.test issues.exp.test.stderr infer-out $(CLEAN_EXTRA)
