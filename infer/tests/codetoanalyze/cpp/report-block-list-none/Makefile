# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

# Same sources as ../report-block-list, with the report block list switched off. This
# records which instantiation de-duplication keeps when nothing is filtered, which is
# what the entries in ../report-block-list/.inferconfig are written against.

ROOT_DIR = ../../../../..
include $(ROOT_DIR)/Makefile.config

default: compile

.PHONY: print replace test clean
print replace test clean:
	$(QUIET)INFER_ARGS=--report-block-list-spec^[] $(MAKE) -C ../report-block-list \
	  TEST_SUFFIX=-none TEST_RESULT_SUFFIX=-none ROOT_REL_SUFFIX=-none $@
