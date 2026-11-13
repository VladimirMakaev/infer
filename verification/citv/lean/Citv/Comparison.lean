/-
Copyright (c) Facebook, Inc. and its affiliates.

This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
-/

/-!
# The comparison fragment of the concrete interval domain

Pulse decides that a comparison between two intervals is impossible, and prunes the path when it
is, in `abduce_binop_is_true` of `infer/src/pulse/PulseCItv.ml`. It also tightens each operand
when the comparison holds. This file models that decision for the case where both operands are
ordinary intervals, and proves that both halves are sound: a comparison called impossible really
has no witness, and a tightened operand still holds every value that takes part in a witness.

The companion bounded check in the parent directory tests the whole module over a window of
endpoints. What is proved here holds for all integers and all bounds, including the infinite
ones, but for one branch of the module rather than all of it.

## Correspondence with the OCaml

Each definition below names the OCaml it mirrors, and the pattern order is kept the same,
because the module's `Bound.le` and `Bound.lt` are not the orders one would write from scratch:
their first two arms make `lt MinusInfinity MinusInfinity` and `lt PlusInfinity PlusInfinity`
both true. Reproducing that faithfully matters more than tidying it, since the theorems are
only about the module if the model is the module. `README.md` has the table.

What is covered: `abduce_le` and `abduce_lt` on two `Between` operands, and the dispatch in
`abduce_binop_constraints` from the four ordered comparison operators onto them. What is not:
the `Outside` operands, `abduce_eq` and `abduce_ne`, the `negated` flag, and the arithmetic.
Those branches are excluded by the hypotheses of the theorems rather than assumed away.
-/

namespace Citv

/-- `Bound.t` of `PulseCItv.ml`: an endpoint is an integer or an infinity. -/
inductive Bound where
  | minusInfinity
  | int (i : Int)
  | plusInfinity
  deriving DecidableEq, Repr

namespace Bound

/-- `Bound.le`. The arm order is the module's: the first two arms fire before the rest, so
`le MinusInfinity MinusInfinity` and `le PlusInfinity PlusInfinity` are both true. -/
def le : Bound → Bound → Bool
  | minusInfinity, _ => true
  | _, plusInfinity => true
  | int _, minusInfinity => false
  | plusInfinity, minusInfinity => false
  | plusInfinity, int _ => false
  | int a, int b => decide (a ≤ b)

/-- `Bound.lt`, with the same arm order, so it agrees with `le` at the infinities. -/
def lt : Bound → Bound → Bool
  | minusInfinity, _ => true
  | _, plusInfinity => true
  | int _, minusInfinity => false
  | plusInfinity, minusInfinity => false
  | plusInfinity, int _ => false
  | int a, int b => decide (a < b)

/-- `Bound.min`. -/
def min (a b : Bound) : Bound := if le a b then a else b

/-- `Bound.max`. -/
def max (a b : Bound) : Bound := if le a b then b else a

/-- `Bound.add_int`, which leaves the infinities alone. -/
def addInt : Bound → Int → Bound
  | minusInfinity, _ => minusInfinity
  | plusInfinity, _ => plusInfinity
  | int i, k => int (i + k)

/-- The lower endpoint admits `x`: the left half of membership in `Between (lo, hi)`. -/
def allowsBelow : Bound → Int → Prop
  | minusInfinity, _ => True
  | int l, x => l ≤ x
  | plusInfinity, _ => False

/-- The upper endpoint admits `x`: the right half of membership in `Between (lo, hi)`. -/
def allowsAbove : Bound → Int → Prop
  | plusInfinity, _ => True
  | int u, x => x ≤ u
  | minusInfinity, _ => False

/-! ### The three facts the soundness proofs rest on

`min` and `max` are defined by an `if` on a comparison that is not a literal, so nothing reduces
them on its own; these say the only thing the proofs need, which is that each picks one of its
two arguments. The monotonicity lemmas are what lets a fact about `y` be moved to `x` across
`x ≤ y`, and `lt_excludes` is the whole content of the impossibility test. -/

theorem min_cases (a b : Bound) : min a b = a ∨ min a b = b := by
  unfold min; split <;> simp

theorem max_cases (a b : Bound) : max a b = a ∨ max a b = b := by
  unfold max; split <;> simp

/-- An upper endpoint that admits `y` admits everything below it. -/
theorem allowsAbove_mono {b : Bound} {x y : Int} (h : allowsAbove b y) (hxy : x ≤ y) :
    allowsAbove b x := by
  cases b <;> simp_all [allowsAbove] <;> omega

/-- A lower endpoint that admits `x` admits everything above it. -/
theorem allowsBelow_mono {b : Bound} {x y : Int} (h : allowsBelow b x) (hxy : x ≤ y) :
    allowsBelow b y := by
  cases b <;> simp_all [allowsBelow] <;> omega

/-- If `b1 < b2` then no integer is admitted from above by `b1` and from below by `b2` at once.
This covers the infinite endpoints as well, which is where the module's unusual arm order has to
be taken at its word: `lt plusInfinity plusInfinity` is true, and the statement still holds
because `plusInfinity` admits nothing from below. -/
theorem lt_excludes {b1 b2 : Bound} {x : Int} (h : lt b1 b2 = true) (ha : allowsAbove b1 x)
    (hb : allowsBelow b2 x) : False := by
  cases b1 <;> cases b2 <;> simp_all [lt, allowsAbove, allowsBelow] <;> omega

/-! ### Shifting a bound by one

`abduce_lt a1 a2` is `abduce_le (a1 + 1) a2` with the left refinement shifted back, so the two
directions of `add_int` by one are needed on both halves of membership. -/

theorem allowsBelow_succ (b : Bound) (x : Int) :
    allowsBelow (addInt b 1) (x + 1) ↔ allowsBelow b x := by
  cases b <;> simp [addInt, allowsBelow] <;> omega

theorem allowsAbove_succ (b : Bound) (x : Int) :
    allowsAbove (addInt b 1) (x + 1) ↔ allowsAbove b x := by
  cases b <;> simp [addInt, allowsAbove] <;> omega

theorem allowsBelow_pred (b : Bound) (x : Int) :
    allowsBelow (addInt b (-1)) x ↔ allowsBelow b (x + 1) := by
  cases b <;> simp [addInt, allowsBelow] <;> omega

theorem allowsAbove_pred (b : Bound) (x : Int) :
    allowsAbove (addInt b (-1)) x ↔ allowsAbove b (x + 1) := by
  cases b <;> simp [addInt, allowsAbove] <;> omega

end Bound

/-- The membership predicate for `Between (lo, hi)`. `Outside` is not modelled here; the
theorems below are stated for `Between` operands only. -/
def memBetween (lo hi : Bound) (x : Int) : Prop :=
  Bound.allowsBelow lo x ∧ Bound.allowsAbove hi x

/-- `PulseCItv.abduction_result`, with the refinements kept as the interval's two endpoints
since only `Between` results arise in the fragment modelled here. -/
inductive Abduced where
  | unsat
  | sat (lo1 hi1 lo2 hi2 : Bound)
  deriving DecidableEq, Repr

/-- `abduce_le` on two `Between` operands. The OCaml names `min u1 u2` and `max l1 l2` with a
`let`; they are written out here so that the guard and the refinement are visibly the same two
expressions, as they are there. -/
def abduceLe (lo1 hi1 lo2 hi2 : Bound) : Abduced :=
  if Bound.lt (Bound.min hi1 hi2) lo1 || Bound.lt hi2 (Bound.max lo1 lo2) then .unsat
  else .sat lo1 (Bound.min hi1 hi2) (Bound.max lo1 lo2) hi2

/-! ### Soundness of the impossibility decision -/

/-- If `abduce_le` reports that `x ≤ y` cannot hold, then no pair drawn from the two intervals
satisfies it. This is the half that prunes a path, so an error here deletes reachable states. -/
theorem abduceLe_unsat_sound {lo1 hi1 lo2 hi2 : Bound}
    (h : abduceLe lo1 hi1 lo2 hi2 = .unsat) :
    ∀ x y, memBetween lo1 hi1 x → memBetween lo2 hi2 y → ¬x ≤ y := by
  intro x y ⟨hx1, hx2⟩ ⟨hy1, hy2⟩ hxy
  simp only [abduceLe] at h
  split at h
  · rename_i hcond
    simp only [Bool.or_eq_true] at hcond
    rcases hcond with hc | hc
    · -- the left interval ends below where it starts, once cut down to the right one
      refine Bound.lt_excludes hc ?_ hx1
      rcases Bound.min_cases hi1 hi2 with he | he <;> rw [he]
      · exact hx2
      · exact Bound.allowsAbove_mono hy2 hxy
    · -- the right interval ends below where the pair would have to start
      refine Bound.lt_excludes hc hy2 ?_
      rcases Bound.max_cases lo1 lo2 with he | he <;> rw [he]
      · exact Bound.allowsBelow_mono hx1 hxy
      · exact hy1
  · simp at h

/-! ### Soundness of the tightening -/

/-- If `abduce_le` accepts and tightens, every pair that satisfies `x ≤ y` is still inside the
tightened intervals. This is the half that narrows an operand, so an error here silently drops
states that the caller still has to consider. -/
theorem abduceLe_sat_sound {lo1 hi1 lo2 hi2 lo1' hi1' lo2' hi2' : Bound}
    (h : abduceLe lo1 hi1 lo2 hi2 = .sat lo1' hi1' lo2' hi2') :
    ∀ x y, memBetween lo1 hi1 x → memBetween lo2 hi2 y → x ≤ y →
      memBetween lo1' hi1' x ∧ memBetween lo2' hi2' y := by
  intro x y ⟨hx1, hx2⟩ ⟨hy1, hy2⟩ hxy
  simp only [abduceLe] at h
  split at h
  · simp at h
  · injection h with e1 e2 e3 e4
    subst e1; subst e2; subst e3; subst e4
    refine ⟨⟨hx1, ?_⟩, ?_, hy2⟩
    · rcases Bound.min_cases hi1 hi2 with he | he <;> rw [he]
      · exact hx2
      · exact Bound.allowsAbove_mono hy2 hxy
    · rcases Bound.max_cases lo1 lo2 with he | he <;> rw [he]
      · exact Bound.allowsBelow_mono hx1 hxy
      · exact hy1

/-! ### The strict comparison

`abduce_lt a1 a2` is `abduce_le (a1 + 1) a2`, with the left refinement shifted back by one. -/

/-- Shifting the left refinement back, which is what the OCaml does to the pair it got from
`abduce_le`. -/
def Abduced.shiftLeftBack : Abduced → Abduced
  | .unsat => .unsat
  | .sat a b c d => .sat (Bound.addInt a (-1)) (Bound.addInt b (-1)) c d

/-- `abduce_lt` on two `Between` operands. -/
def abduceLt (lo1 hi1 lo2 hi2 : Bound) : Abduced :=
  (abduceLe (Bound.addInt lo1 1) (Bound.addInt hi1 1) lo2 hi2).shiftLeftBack

/-- `x` is in `[lo1, hi1]` exactly when `x + 1` is in the shifted interval, which is what makes
the reduction to `abduce_le` faithful rather than merely plausible. -/
theorem memBetween_succ (lo hi : Bound) (x : Int) :
    memBetween (Bound.addInt lo 1) (Bound.addInt hi 1) (x + 1) ↔ memBetween lo hi x := by
  unfold memBetween
  rw [Bound.allowsBelow_succ, Bound.allowsAbove_succ]

/-- If `abduce_lt` reports that `x < y` cannot hold, then no pair satisfies it. -/
theorem abduceLt_unsat_sound {lo1 hi1 lo2 hi2 : Bound}
    (h : abduceLt lo1 hi1 lo2 hi2 = .unsat) :
    ∀ x y, memBetween lo1 hi1 x → memBetween lo2 hi2 y → ¬x < y := by
  intro x y hx hy hxy
  simp only [abduceLt] at h
  cases heq : abduceLe (Bound.addInt lo1 1) (Bound.addInt hi1 1) lo2 hi2 with
  | unsat =>
      exact abduceLe_unsat_sound heq (x + 1) y ((memBetween_succ lo1 hi1 x).mpr hx) hy (by omega)
  | sat a b c d =>
      rw [heq] at h
      simp [Abduced.shiftLeftBack] at h

/-- If `abduce_lt` accepts and tightens, every pair that satisfies `x < y` survives. -/
theorem abduceLt_sat_sound {lo1 hi1 lo2 hi2 lo1' hi1' lo2' hi2' : Bound}
    (h : abduceLt lo1 hi1 lo2 hi2 = .sat lo1' hi1' lo2' hi2') :
    ∀ x y, memBetween lo1 hi1 x → memBetween lo2 hi2 y → x < y →
      memBetween lo1' hi1' x ∧ memBetween lo2' hi2' y := by
  intro x y hx hy hxy
  simp only [abduceLt] at h
  cases heq : abduceLe (Bound.addInt lo1 1) (Bound.addInt hi1 1) lo2 hi2 with
  | unsat =>
      rw [heq] at h
      simp [Abduced.shiftLeftBack] at h
  | sat a b c d =>
      rw [heq] at h
      simp only [Abduced.shiftLeftBack] at h
      injection h with e1 e2 e3 e4
      subst e1; subst e2; subst e3; subst e4
      obtain ⟨⟨p1, p2⟩, q⟩ :=
        abduceLe_sat_sound heq (x + 1) y ((memBetween_succ lo1 hi1 x).mpr hx) hy (by omega)
      exact ⟨⟨(Bound.allowsBelow_pred a x).mpr p1, (Bound.allowsAbove_pred b x).mpr p2⟩, q⟩

/-! ### Dispatch

`abduce_binop_constraints` maps the comparison operators onto the two helpers above. `Ge` and
`Gt` reach them with the operands swapped, and then pass the answer through `flip_abduced`,
which puts the two refinements back in the caller's order. That last step is not cosmetic: for
`[2,2] >= [1,1]` the source answers lhs `{2}`, rhs `{1}`, and a model that swapped the operands
without flipping back would answer `{1}`, `{2}`. It is modelled here, and pinned by a witness
at the bottom of this section. -/

/-- The ordered comparison operators of `Binop.t`. -/
inductive Cmp where
  | le | lt | ge | gt
  deriving DecidableEq, Repr

/-- The concrete meaning of a comparison, used to state what the decision is about. -/
def Cmp.holds : Cmp → Int → Int → Prop
  | le, x, y => x ≤ y
  | lt, x, y => x < y
  | ge, x, y => x ≥ y
  | gt, x, y => x > y

/-- `flip_abduced`: the two refinements change places, and an impossibility stays one. -/
def Abduced.flip : Abduced → Abduced
  | .unsat => .unsat
  | .sat a b c d => .sat c d a b

theorem Abduced.flip_unsat {r : Abduced} (h : r.flip = .unsat) : r = .unsat := by
  cases r <;> simp [Abduced.flip] at h ⊢

theorem Abduced.flip_sat {r : Abduced} {a b c d : Bound} (h : r.flip = .sat a b c d) :
    r = .sat c d a b := by
  cases r with
  | unsat => simp [Abduced.flip] at h
  | sat p q u v =>
      simp only [Abduced.flip] at h
      injection h with e1 e2 e3 e4
      subst e1; subst e2; subst e3; subst e4
      rfl

/-- `abduce_binop_constraints` for the ordered operators with `negated = false`. `Ge` and `Gt`
swap the operands and flip the answer back, exactly as the OCaml does. -/
def abduceCmp (c : Cmp) (lo1 hi1 lo2 hi2 : Bound) : Abduced :=
  match c with
  | .le => abduceLe lo1 hi1 lo2 hi2
  | .lt => abduceLt lo1 hi1 lo2 hi2
  | .ge => (abduceLe lo2 hi2 lo1 hi1).flip
  | .gt => (abduceLt lo2 hi2 lo1 hi1).flip

/-- The top-level statement for the fragment: whenever the dispatch decides a comparison between
two ordinary intervals is impossible, it really is. -/
theorem abduceCmp_unsat_sound {c : Cmp} {lo1 hi1 lo2 hi2 : Bound}
    (h : abduceCmp c lo1 hi1 lo2 hi2 = .unsat) :
    ∀ x y, memBetween lo1 hi1 x → memBetween lo2 hi2 y → ¬c.holds x y := by
  intro x y hx hy hc
  simp only [abduceCmp] at h
  cases c
  · exact abduceLe_unsat_sound h x y hx hy hc
  · exact abduceLt_unsat_sound h x y hx hy hc
  · exact abduceLe_unsat_sound (Abduced.flip_unsat h) y x hy hx hc
  · exact abduceLt_unsat_sound (Abduced.flip_unsat h) y x hy hx hc


/-- And the other half for the dispatch: a tightened pair comes back in the caller's order, so
this is the statement the flip is needed for. -/
theorem abduceCmp_sat_sound {c : Cmp} {lo1 hi1 lo2 hi2 lo1' hi1' lo2' hi2' : Bound}
    (h : abduceCmp c lo1 hi1 lo2 hi2 = .sat lo1' hi1' lo2' hi2') :
    ∀ x y, memBetween lo1 hi1 x → memBetween lo2 hi2 y → c.holds x y →
      memBetween lo1' hi1' x ∧ memBetween lo2' hi2' y := by
  intro x y hx hy hc
  simp only [abduceCmp] at h
  cases c
  · exact abduceLe_sat_sound h x y hx hy hc
  · exact abduceLt_sat_sound h x y hx hy hc
  · exact (abduceLe_sat_sound (Abduced.flip_sat h) y x hy hx hc).symm
  · exact (abduceLt_sat_sound (Abduced.flip_sat h) y x hy hx hc).symm


/-- The witness that pins the flip. `[2,2] >= [1,1]` tightens the left operand to `{2}` and the
right to `{1}`, in that order. Without `flip_abduced` the two come back the other way round, and
a model that dropped it would still prove every theorem above except this one. -/
theorem abduceCmp_ge_puts_the_refinements_in_the_callers_order :
    abduceCmp .ge (.int 2) (.int 2) (.int 1) (.int 1)
    = .sat (.int 2) (.int 2) (.int 1) (.int 1) := by
  decide

/-! ### A variant that is not sound, and is proved not to be

A proof that holds of a model no defect can break says nothing about the module. This is the
mutant `abduce_le/declare-unsat-too-eagerly` from `mutants/mutants.txt`: the first disjunct of
the guard uses `le` where the module uses `lt`. The statement proved here is the negation of the
soundness statement above, so it fails if the two definitions ever converge. -/

/-- `abduce_le` with `le` in place of `lt` in the first half of the guard. -/
def abduceLeEager (lo1 hi1 lo2 hi2 : Bound) : Abduced :=
  if Bound.le (Bound.min hi1 hi2) lo1 || Bound.lt hi2 (Bound.max lo1 lo2) then .unsat
  else .sat lo1 (Bound.min hi1 hi2) (Bound.max lo1 lo2) hi2

/-- The eager guard calls `[0,0] ≤ [0,0]` impossible, which `(0, 0)` satisfies. -/
theorem abduceLeEager_not_unsat_sound :
    ¬∀ lo1 hi1 lo2 hi2 : Bound,
        abduceLeEager lo1 hi1 lo2 hi2 = .unsat →
          ∀ x y, memBetween lo1 hi1 x → memBetween lo2 hi2 y → ¬x ≤ y := by
  intro hsound
  have zero_in : memBetween (.int 0) (.int 0) 0 := by
    simp [memBetween, Bound.allowsBelow, Bound.allowsAbove]
  exact hsound (.int 0) (.int 0) (.int 0) (.int 0) (by decide) 0 0 zero_in zero_in (by decide)

end Citv
