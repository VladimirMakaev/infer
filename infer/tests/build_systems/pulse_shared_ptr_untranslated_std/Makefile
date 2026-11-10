# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

TESTS_DIR = ../..

# see explanations in cpp/pulse/Makefile for the custom isystem
CLANG_OPTIONS = -x c++ -std=c++17 -nostdinc++ -isystem$(CLANG_INCLUDES)/c++/v1/ -c

# --skip-translation-headers keeps the standard library's bodies out of the capture, which is
# the state a real build is usually in. std::vector's insertions then have no body to analyse
# and the container model builds the new element itself, by looking the element's constructor
# up and applying it.
#
# WHAT THIS SUITE COVERS, and what it does not, because the difference is not obvious and it
# decides which cases are worth writing here.
#
# The lookup reads constructor formal types out of the capture. It is NOT defeated by the flag
# above: the type environment keeps std::shared_ptr either way, and its default, copy, move and
# nullptr constructors all stay recorded. So an insertion whose own parameter type matches one
# of those constructors is built by the normal path:
#
#   push_back(const T&) -> matches shared_ptr(const shared_ptr&)
#   push_back(T&&)      -> matches shared_ptr(shared_ptr&&)
#   emplace_back(T&)    -> matches shared_ptr(const shared_ptr&)
#
# and a shared_ptr-to-shared_ptr copy or move through those cannot be tested here at all: it
# behaves identically however the fallback is written. Only an insertion whose parameter type
# matches no constructor reaches the fallback, which in practice means a source that is not
# already a std::shared_ptr:
#
#   emplace_back(std::unique_ptr<T, D>&&) -> matches nothing, so the fallback runs
#
# That is what the cases below exercise, and it is the property worth pinning: the fallback must
# not hand std::shared_ptr's ownership semantics to a source that is not one, because such a
# source has no reference count to read and carries a deleter that would be lost.
#
# Covering the shared_ptr-to-shared_ptr paths needs a capture whose type environment has no
# std::shared_ptr at all, i.e. a hand written stand-in header rather than this flag.
INFER_OPTIONS = \
  --pulse-only --debug-exceptions --no-pulse-force-continue \
  --skip-translation-headers '.*/v1/' \
  --project-root $(TESTS_DIR)

INFERPRINT_OPTIONS = --issues-tests

SOURCES = $(wildcard *.cpp)

include $(TESTS_DIR)/clang.make

infer-out/report.json: $(MAKEFILE_LIST)
