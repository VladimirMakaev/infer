# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

TESTS_DIR = ../..

# Two translation units that disagree about the signedness of plain `char` instantiate the two
# halves of one recursive template pair. They mangle alike, so the capture database holds a
# single entry for each half, and the analysis of one half re-enters the other under the name
# the other unit gave it. Before the ondemand fix this ends the analysis with
# "Hash_queue.enqueue_exn: duplicate key" instead of being recognised as the recursion it is.
INFER_OPTIONS = --developer-mode
SOURCES = \
  ../codetoanalyze/char_signedness/ping_unit.cpp \
  ../codetoanalyze/char_signedness/pong_unit.cpp
CLEAN_EXTRA = compile_commands.json
INFERPRINT_OPTIONS = --issues-tests

include $(TESTS_DIR)/infer.make

compile_commands.json: compile_commands.json.in
	$(QUIET)sed -e 's#%pwd%#$(CURDIR)#g' $< > $@ || $(REMOVE) $@

infer-out/report.json: compile_commands.json $(SOURCES) Makefile $(CLANG_DEPS)
	$(QUIET)$(call silent_on_success,Clang compilation database with mixed char signedness,\
	  $(INFER_BIN) $(INFER_OPTIONS) --project-root $(TESTS_DIR) --compilation-database $<)
