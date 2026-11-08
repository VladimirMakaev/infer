# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

TESTS_DIR = ../..
SRC_REL = ../codetoanalyze/copy_batch_independence

# `res` is copied into the intermediate that initialises the base class and never used again,
# so the copy can be a move. Capturing the two units that hold the base constructors makes the
# leaf constructor have no normal exit -- GraphicsContext's constructor never returns -- and
# before the fix that silenced the report for the whole procedure. The four runs below capture
# every combination of those two units and analyse only leaves.cpp: all four must agree.
INFER_OPTIONS = --developer-mode --pulse-only --pulse-prevent-non-disj-top
SOURCES = \
  $(SRC_REL)/leaves.cpp $(SRC_REL)/renderer_base.cpp $(SRC_REL)/context.cpp
COMBOS = neither base middle both
CLEAN_EXTRA = $(foreach c,$(COMBOS),cdb-$(c).json infer-out-$(c)) issues.exp.test.combos
INFERPRINT_OPTIONS = --issues-tests

include $(TESTS_DIR)/infer.make

UNITS_neither = leaves.cpp
UNITS_base    = leaves.cpp renderer_base.cpp
UNITS_middle  = leaves.cpp context.cpp
UNITS_both    = leaves.cpp renderer_base.cpp context.cpp

infer-out/report.json:
#	this test defines issues.exp.test directly from the four runs below

define combo_rule
cdb-$(1).json: compile_commands.json.in
	$$(QUIET)$$(call silent_on_success,Building compilation database $(1),\
	  sed -e 's#%src%#$$(CURDIR)/$$(SRC_REL)#g' $$< \
	  | python3 -c "import json,sys; units=set('$$(UNITS_$(1))'.split()); json.dump([e for e in json.load(sys.stdin) if e['file'] in units], sys.stdout, indent=1)" \
	  > $$@)

infer-out-$(1)/report.json: cdb-$(1).json $$(SOURCES) Makefile $$(CLANG_DEPS)
	$$(QUIET)$$(call silent_on_success,Copy report with $(1) base unit(s) captured,\
	  $$(INFER_BIN) $$(INFER_OPTIONS) --project-root $$(TESTS_DIR) -o $$(@D) \
	    --compilation-database cdb-$(1).json)
endef
$(foreach c,$(COMBOS),$(eval $(call combo_rule,$(c))))

issues.exp.test: $(foreach c,$(COMBOS),infer-out-$(c)/report.json)
	$(QUIET)$(REMOVE) $@
	$(QUIET)$(foreach c,$(COMBOS), \
	  echo "--- captured: $(UNITS_$(c))" >> $@; \
	  $(INFER_BIN) report -q $(INFERPRINT_OPTIONS) /dev/stdout -o infer-out-$(c) \
	    | grep -F copy_batch_independence >> $@ || true; )
