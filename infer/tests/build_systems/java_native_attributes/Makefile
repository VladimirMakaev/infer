# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

TESTS_DIR = ../..

include $(TESTS_DIR)/base.make

.PHONY: test clean
test: attributes.test merged-attributes.test check.py
	python3 check.py attributes.test infer-out/report.json
	python3 check.py merged-attributes.test infer-out/report.json

attributes.test: NativeMethods.java check.py $(INFER_BIN) $(MAKEFILE_LIST)
	$(INFER_BIN) --pulse-only --project-root . -o infer-out -- $(JAVAC) -g NativeMethods.java
	$(INFER_BIN) debug -o infer-out --procedures --procedures-attributes > $@

merged-attributes.test: attributes.test
	$(INFER_BIN) capture --project-root . -o infer-out-merged --merge-capture $$(realpath infer-out)
	$(INFER_BIN) debug -o infer-out-merged --procedures --procedures-attributes > $@

clean:
	$(REMOVE_DIR) infer-out infer-out-merged codetoanalyze *.test
