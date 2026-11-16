# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

TESTS_DIR = ../..

include $(TESTS_DIR)/base.make

.PHONY: test clean
test:
	python3 check.py $(INFER_BIN)

clean:
	$(REMOVE_DIR) infer-out-*
