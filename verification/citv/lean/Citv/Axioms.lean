/-
Copyright (c) Facebook, Inc. and its affiliates.

This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
-/
import Citv.Comparison

/-!
# What the theorems rest on

`#print axioms` for every result proved in this development, the small lemmas included. A proof
that quietly grows an assumption, or that is finished with `sorry`, shows up here as an axiom
the whitelist does not allow.

`check-axioms.sh` builds this file from clean and reads these lines back. It fails on any axiom
outside {`propext`, `Quot.sound`, `Classical.choice`}, on any mention of `sorry`, and — because
a build that printed nothing would otherwise pass — unless it sees exactly as many reports as
there are `#print axioms` commands below.
-/

#print axioms Citv.Bound.min_cases
#print axioms Citv.Bound.max_cases
#print axioms Citv.Bound.allowsAbove_mono
#print axioms Citv.Bound.allowsBelow_mono
#print axioms Citv.Bound.lt_excludes
#print axioms Citv.Bound.allowsBelow_succ
#print axioms Citv.Bound.allowsAbove_succ
#print axioms Citv.Bound.allowsBelow_pred
#print axioms Citv.Bound.allowsAbove_pred
#print axioms Citv.memBetween_succ
#print axioms Citv.abduceLe_unsat_sound
#print axioms Citv.abduceLe_sat_sound
#print axioms Citv.abduceLt_unsat_sound
#print axioms Citv.abduceLt_sat_sound
#print axioms Citv.abduceCmp_unsat_sound
#print axioms Citv.abduceLeEager_not_unsat_sound
#print axioms Citv.Abduced.flip_unsat
#print axioms Citv.Abduced.flip_sat
#print axioms Citv.abduceCmp_sat_sound
#print axioms Citv.abduceCmp_ge_puts_the_refinements_in_the_callers_order
