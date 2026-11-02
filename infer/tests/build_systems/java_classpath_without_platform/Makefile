# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

# javac runs first, outside infer, and infer captures what it produced. The classpath infer is
# given is the one a build system passes: the libraries the target compiles against, and nothing
# that holds the platform classes. Infer has to find those itself, or every model keyed on a
# platform supertype stops matching.

TESTS_DIR = ../..

INFER_OPTIONS = --pulse-only --debug-exceptions --no-pulse-force-continue
INFERPRINT_OPTIONS = --issues-tests
SOURCES = Streams.java
CLASSES_DIR = classes
CLEAN_EXTRA = $(CLASSES_DIR) *.class

include $(TESTS_DIR)/java.make
include $(TESTS_DIR)/infer.make

$(INFER_OUT)/report.json: $(JAVA_DEPS) $(SOURCES) $(MAKEFILE_LIST)
	$(QUIET)$(REMOVE_DIR) $(CLASSES_DIR)
	$(QUIET)$(MKDIR_P) $(CLASSES_DIR)
	$(QUIET)$(JAVAC) -g --release 21 -cp "$(CLASSPATH)" -d $(CLASSES_DIR) $(SOURCES)
	$(QUIET)$(call silent_on_success,Testing a classpath without platform classes in $(TEST_REL_DIR),\
	  $(INFER_BIN) capture --project-root $(TESTS_DIR) -o $(@D) $(INFER_OPTIONS) \
	    --generated-classes $(CLASSES_DIR) --classpath "$(INFER_ANNOTATIONS_JAR)" \
	    --sources $(SOURCES) && \
	  $(INFER_BIN) analyze --project-root $(TESTS_DIR) -o $(@D) $(INFER_OPTIONS))
