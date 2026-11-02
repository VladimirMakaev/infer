# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

# Captures already-compiled Kotlin through --generated-classes, the way a build system that
# hands Infer a library's compiled output does, instead of running the compiler under Infer.
# The two modes do not see the same program -- capturing the output leaves the compiler's own
# helpers outside the analysis -- and several Kotlin shapes only report in this one, so a
# fixture for them is silent in the other Kotlin tests and belongs here.

TESTS_DIR = ../../..

INFER_OPTIONS = --pulse-only --debug-exceptions --no-pulse-force-continue
INFERPRINT_OPTIONS = --issues-tests

SOURCES = $(sort $(wildcard *.kt))
CLASSES_DIR = classes

CLEAN_EXTRA = $(CLASSES_DIR) *.class META-INF

include $(TESTS_DIR)/infer.make
include $(TESTS_DIR)/java.make

$(INFER_OUT)/report.json: $(JAVA_DEPS) $(SOURCES) $(MAKEFILE_LIST)
	$(QUIET)$(REMOVE_DIR) $(CLASSES_DIR)
	$(QUIET)$(MKDIR_P) $(CLASSES_DIR)
	$(QUIET)$(KOTLINC) -nowarn -cp "$(CLASSPATH)" -d $(CLASSES_DIR) $(SOURCES)
	$(QUIET)$(call silent_on_success,Testing Kotlin compiled-output capture in $(TEST_REL_DIR),\
	  $(INFER_BIN) capture -o $(@D) $(INFER_OPTIONS) \
	    --generated-classes $(CLASSES_DIR) --classpath "$(CLASSPATH)" \
	    $(foreach source,$(SOURCES),--sources $(source)) && \
	  $(INFER_BIN) analyze -o $(@D) $(INFER_OPTIONS))
