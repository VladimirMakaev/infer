# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

# A capture whose process did not finish records the file it was translating and then stores its
# procedures one by one, so it can leave a caller behind without the callee it calls. The model
# of std::make_shared is chosen by the callee's name and read the constructor's parameter types
# from that callee's formals, so it died on the call and the whole run lost its results. The call
# is now treated as a call to a procedure that was not captured, and the rest of the file is
# still analysed and reported.

TESTS_DIR = ../..

CLANG_OPTIONS = -x c++ -std=c++17 -c
INFER_OPTIONS = --pulse-only --project-root $(PROJECT_ROOT)
INFERPRINT_OPTIONS = --issues-tests

SOURCES = future_manager.cpp

include $(TESTS_DIR)/infer.make

PROJECT_ROOT ?= $(TESTS_DIR)

infer-out$(TEST_SUFFIX)/report.json: $(CLANG_DEPS) $(SOURCES) $(MAKEFILE_LIST) drop_make_shared_declaration.py
	$(QUIET)$(call silent_on_success,Capturing future_manager.cpp,\
	  $(INFER_BIN) capture --project-root $(PROJECT_ROOT) -o $(@D) -- \
	    clang $(CLANG_OPTIONS) future_manager.cpp -o /dev/null)
#	the file stays recorded and the constructor keeps its body; the declaration of the
#	std::make_shared it calls is the row the capture never got to write
	$(QUIET)$(call silent_on_success,Dropping the declaration of the callee,\
	  python3 ./drop_make_shared_declaration.py $(@D)/capture.db)
	$(QUIET)$(call silent_on_success,Testing infer/clang in $(TEST_REL_DIR),\
	  $(INFER_BIN) analyze --results-dir $(@D) $(INFER_OPTIONS))
