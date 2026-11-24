(** Soundness of PulseCItv's ordered-comparison fragment, proved on the file that
    rocq-of-ocaml generated from Infer's real PulseCItv.ml.

    The concrete values here are IntLit.t - the tagged literals the analyser actually
    manipulates - ordered by the module's OWN order, not by the order on Z.  That matters:
    IntLit compares by (area, i) where area is 1 for negative Signed, 2 for non-negative and
    3 for negative UNSIGNED, so a negative unsigned literal is GREATER than every
    non-negative one.  An earlier version of this development modelled IntLit.t as plain Z
    and therefore proved theorems about a different module; the two witnesses at the bottom
    of this file are the executed counterexamples, kept as regression checks. *)

Require Import RocqOfOCaml.RocqOfOCaml.
Require Import RocqOfOCaml.Settings.
From Stdlib Require Import ZArith.
Require Import IntLit.
Require Import PulseCItv.
From Stdlib Require Import Lia.

Local Open Scope Z_scope.

(** * Reflecting the generated boolean comparisons on Z

    The generated code compares through RocqOfOCaml.Stdlib.le/lt/ge/gt, which dispatch on
    the OrderDec instance for Z, i.e. Z.compare.

    These are proved by hand rather than by lia because Z.lt is DEFINITIONALLY
    (x ?= y) = Lt, so [apply Z.compare_lt_iff] leaves the hypothesis in its unfolded
    comparison form and zify - which matches syntactically - then cannot see an inequality
    at all, failing with "Cannot find witness". *)

Lemma bridge : forall a b : Z, compare a b = Z.compare a b.
Proof. reflexivity. Qed.

Lemma sle_true : forall a b : Z, RocqOfOCaml.Stdlib.le a b = true -> a <= b.
Proof.
  intros a b H. unfold RocqOfOCaml.Stdlib.le in H. rewrite bridge in H.
  destruct (Z.compare a b) eqn:E; try discriminate.
  - apply Z.compare_eq_iff in E. rewrite E. apply Z.le_refl.
  - apply Z.compare_lt_iff in E. exact (Z.lt_le_incl a b E).
Qed.

Lemma sle_false : forall a b : Z, RocqOfOCaml.Stdlib.le a b = false -> b < a.
Proof.
  intros a b H. unfold RocqOfOCaml.Stdlib.le in H. rewrite bridge in H.
  destruct (Z.compare a b) eqn:E; try discriminate.
  apply Z.compare_gt_iff in E. exact E.
Qed.

Lemma slt_true : forall a b : Z, RocqOfOCaml.Stdlib.lt a b = true -> a < b.
Proof.
  intros a b H. unfold RocqOfOCaml.Stdlib.lt in H. rewrite bridge in H.
  destruct (Z.compare a b) eqn:E; try discriminate.
  apply Z.compare_lt_iff in E. exact E.
Qed.

Lemma slt_false : forall a b : Z, RocqOfOCaml.Stdlib.lt a b = false -> b <= a.
Proof.
  intros a b H. unfold RocqOfOCaml.Stdlib.lt in H. rewrite bridge in H.
  destruct (Z.compare a b) eqn:E; try discriminate.
  - apply Z.compare_eq_iff in E. rewrite E. apply Z.le_refl.
  - apply Z.compare_gt_iff in E. exact (Z.lt_le_incl b a E).
Qed.

Lemma sge_true : forall a b : Z, RocqOfOCaml.Stdlib.ge a b = true -> b <= a.
Proof.
  intros a b H. unfold RocqOfOCaml.Stdlib.ge in H. rewrite bridge in H.
  destruct (Z.compare a b) eqn:E; try discriminate.
  - apply Z.compare_eq_iff in E. rewrite E. apply Z.le_refl.
  - apply Z.compare_gt_iff in E. exact (Z.lt_le_incl b a E).
Qed.

Lemma sge_false : forall a b : Z, RocqOfOCaml.Stdlib.ge a b = false -> a < b.
Proof.
  intros a b H. unfold RocqOfOCaml.Stdlib.ge in H. rewrite bridge in H.
  destruct (Z.compare a b) eqn:E; try discriminate.
  apply Z.compare_lt_iff in E. exact E.
Qed.

Lemma sgt_true : forall a b : Z, RocqOfOCaml.Stdlib.gt a b = true -> b < a.
Proof.
  intros a b H. unfold RocqOfOCaml.Stdlib.gt in H. rewrite bridge in H.
  destruct (Z.compare a b) eqn:E; try discriminate.
  apply Z.compare_gt_iff in E. exact E.
Qed.

Lemma sgt_false : forall a b : Z, RocqOfOCaml.Stdlib.gt a b = false -> a <= b.
Proof.
  intros a b H. unfold RocqOfOCaml.Stdlib.gt in H. rewrite bridge in H.
  destruct (Z.compare a b) eqn:E; try discriminate.
  - apply Z.compare_eq_iff in E. rewrite E. apply Z.le_refl.
  - apply Z.compare_lt_iff in E. exact (Z.lt_le_incl a b E).
Qed.

(** * The IntLit order, as the module actually defines it

    [area] is 1 / 2 / 3, and [compare_value] is lexicographic on (area, i).  Everything
    below reduces the module's booleans to that lexicographic pair so that the ordering
    arguments can be done by lia on two Z components. *)

Definition irank (x : IntLit.t) : Z * Z := (IntLit.area x, x.(IntLit.t.i)).

Definition lexle (p q : Z * Z) : Prop :=
  fst p < fst q \/ (fst p = fst q /\ snd p <= snd q).

Definition lexlt (p q : Z * Z) : Prop :=
  fst p < fst q \/ (fst p = fst q /\ snd p < snd q).

(** the order as a Prop, which is what the concretisation below is stated with *)
Definition ile (x y : IntLit.t) : Prop := IntLit.leq x y = true.
Definition ilt (x y : IntLit.t) : Prop := IntLit.lt x y = true.

(** The three areas, and the fact that a negative unsigned sits ABOVE every non-negative. *)
Lemma area_cases : forall x : IntLit.t,
    (IntLit.area x = 1 /\ x.(IntLit.t.i) < 0)
    \/ (IntLit.area x = 2 /\ 0 <= x.(IntLit.t.i))
    \/ (IntLit.area x = 3 /\ x.(IntLit.t.i) < 0).
Proof.
  intros x. unfold IntLit.area.
  destruct (RocqOfOCaml.Stdlib.lt x.(IntLit.t.i) 0) eqn:E;
    destruct x.(IntLit.t.signedness); cbn.
  - left. split; [ reflexivity | exact (slt_true _ _ E) ].
  - right; right. split; [ reflexivity | exact (slt_true _ _ E) ].
  - right; left. split; [ reflexivity | exact (slt_false _ _ E) ].
  - right; left. split; [ reflexivity | exact (slt_false _ _ E) ].
Qed.

(** compare_value, case by case.  Each branch of the generated nest is pinned to the
    lexicographic fact that produced it. *)
Lemma compare_value_spec : forall x y : IntLit.t,
    (IntLit.compare_value x y = (-1) /\ lexlt (irank x) (irank y))
    \/ (IntLit.compare_value x y = 0 /\ fst (irank x) = fst (irank y)
        /\ snd (irank x) = snd (irank y))
    \/ (IntLit.compare_value x y = 1 /\ lexlt (irank y) (irank x)).
Proof.
  intros x y. unfold IntLit.compare_value. cbv zeta.
  unfold irank, lexlt, fst, snd.
  destruct (RocqOfOCaml.Stdlib.lt (IntLit.area x) (IntLit.area y)) eqn:A1.
  { left. split; [ reflexivity | left; exact (slt_true _ _ A1) ]. }
  destruct (RocqOfOCaml.Stdlib.gt (IntLit.area x) (IntLit.area y)) eqn:A2.
  { right; right. split; [ reflexivity | left; exact (sgt_true _ _ A2) ]. }
  pose proof (slt_false _ _ A1) as L1. pose proof (sgt_false _ _ A2) as L2.
  assert (Aeq : IntLit.area x = IntLit.area y) by lia.
  destruct (RocqOfOCaml.Stdlib.lt x.(IntLit.t.i) y.(IntLit.t.i)) eqn:B1.
  { left. split; [ reflexivity | right; split; [ exact Aeq | exact (slt_true _ _ B1) ] ]. }
  destruct (RocqOfOCaml.Stdlib.gt x.(IntLit.t.i) y.(IntLit.t.i)) eqn:B2.
  { right; right. split; [ reflexivity | right; split; [ lia | exact (sgt_true _ _ B2) ] ]. }
  pose proof (slt_false _ _ B1) as M1. pose proof (sgt_false _ _ B2) as M2.
  right; left. split; [ reflexivity | split; [ exact Aeq | lia ] ].
Qed.

Lemma ile_iff : forall x y, ile x y <-> lexle (irank x) (irank y).
Proof.
  intros x y. unfold ile, IntLit.leq.
  destruct (compare_value_spec x y) as [[E H] | [[E [H1 H2]] | [E H]]]; rewrite E;
    split; intro K.
  - unfold lexle, lexlt, irank, fst, snd in *; lia.
  - reflexivity.
  - unfold lexle, irank, fst, snd in *; lia.
  - reflexivity.
  - exfalso. apply sle_true in K. lia.
  - exfalso. unfold lexle, lexlt, irank, fst, snd in *; lia.
Qed.

Lemma ilt_iff : forall x y, ilt x y <-> lexlt (irank x) (irank y).
Proof.
  intros x y. unfold ilt, IntLit.lt.
  destruct (compare_value_spec x y) as [[E H] | [[E [H1 H2]] | [E H]]]; rewrite E;
    split; intro K.
  - exact H.
  - reflexivity.
  - exfalso. apply slt_true in K. lia.
  - exfalso. unfold lexlt, irank, fst, snd in *; lia.
  - exfalso. apply slt_true in K. lia.
  - exfalso. unfold lexlt, irank, fst, snd in *; lia.
Qed.

Lemma ile_false : forall x y, IntLit.leq x y = false -> lexlt (irank y) (irank x).
Proof.
  intros x y H.
  destruct (compare_value_spec x y) as [[E K] | [[E [K1 K2]] | [E K]]];
    unfold IntLit.leq in H; rewrite E in H; cbn in H; try discriminate.
  (* only compare_value = 1 survives: the other two give Stdlib.le _ 0 = true *)
  exact K.
Qed.

Lemma ilt_false : forall x y, IntLit.lt x y = false -> lexle (irank y) (irank x).
Proof.
  intros x y H.
  destruct (compare_value_spec x y) as [[E K] | [[E [K1 K2]] | [E K]]];
    unfold IntLit.lt in H; rewrite E in H; cbn in H; try discriminate;
    unfold lexle, lexlt, irank, fst, snd in *; lia.
Qed.

(** Reduce every ordering hypothesis and goal to lia on the two rank components. *)
Ltac ord :=
  repeat match goal with
         | [ H : ile _ _ |- _ ] => apply ile_iff in H
         | [ H : ilt _ _ |- _ ] => apply ilt_iff in H
         | [ H : IntLit.leq _ _ = true |- _ ] => apply (proj1 (ile_iff _ _)) in H
         | [ H : IntLit.lt _ _ = true |- _ ] => apply (proj1 (ilt_iff _ _)) in H
         | [ H : IntLit.leq _ _ = false |- _ ] => apply ile_false in H
         | [ H : IntLit.lt _ _ = false |- _ ] => apply ilt_false in H
         | [ H : IntLit.geq _ _ = true |- _ ] => unfold IntLit.geq in H
         end;
  try (apply ile_iff); try (apply ilt_iff);
  unfold ile, ilt, lexle, lexlt, irank, fst, snd in *;
  lia.

Lemma ile_refl : forall x, ile x x.
Proof. intros x. ord. Qed.

Lemma ile_trans : forall x y z, ile x y -> ile y z -> ile x z.
Proof. intros x y z A B. ord. Qed.

Lemma ilt_ile : forall x y, ilt x y -> ile x y.
Proof. intros x y A. ord. Qed.

Lemma ile_ilt_trans : forall x y z, ile x y -> ilt y z -> ilt x z.
Proof. intros x y z A B. ord. Qed.

Lemma ilt_ile_trans : forall x y z, ilt x y -> ile y z -> ilt x z.
Proof. intros x y z A B. ord. Qed.

Lemma ilt_irrefl : forall x, ~ ilt x x.
Proof. intros x A. ord. Qed.

Lemma ilt_not_ile : forall x y, ilt x y -> ~ ile y x.
Proof. intros x y A B. ord. Qed.

(** * Adding one, which is where the tags bite

    [IntLit.one] is Signed, and join_signedness Signed s = s, so [add one u] keeps u's tags
    and bumps the value.  That is NOT monotone in IntLit's order: Unsigned(-1) is in area 3
    and Unsigned(0) is in area 2, so adding one to it moves the literal DOWN.  The lemma
    below is still true in the direction abduce_lt needs, and the case analysis is exactly
    where the area boundaries have to be checked. *)

Lemma add_one_i : forall u,
    (IntLit.add IntLit.one u).(IntLit.t.i) = 1 + u.(IntLit.t.i).
Proof. intros u. reflexivity. Qed.

Lemma add_one_sign : forall u,
    (IntLit.add IntLit.one u).(IntLit.t.signedness) = u.(IntLit.t.signedness).
Proof.
  intros u. unfold IntLit.add, IntLit.lift2, IntLit.join_signedness. cbn.
  destruct u.(IntLit.t.signedness); reflexivity.
Qed.

Lemma area_nonneg : forall x : IntLit.t,
    0 <= x.(IntLit.t.i) -> IntLit.area x = 2.
Proof.
  intros x H. unfold IntLit.area.
  destruct (RocqOfOCaml.Stdlib.lt x.(IntLit.t.i) 0) eqn:E.
  - apply slt_true in E. lia.
  - destruct x.(IntLit.t.signedness); reflexivity.
Qed.

Lemma area_neg_signed : forall x : IntLit.t,
    x.(IntLit.t.i) < 0 -> x.(IntLit.t.signedness) = IntLit.Signed -> IntLit.area x = 1.
Proof.
  intros x H Hs. unfold IntLit.area.
  destruct (RocqOfOCaml.Stdlib.lt x.(IntLit.t.i) 0) eqn:E.
  - rewrite Hs. reflexivity.
  - apply slt_false in E. lia.
Qed.

Lemma area_neg_unsigned : forall x : IntLit.t,
    x.(IntLit.t.i) < 0 -> x.(IntLit.t.signedness) = IntLit.Unsigned -> IntLit.area x = 3.
Proof.
  intros x H Hs. unfold IntLit.area.
  destruct (RocqOfOCaml.Stdlib.lt x.(IntLit.t.i) 0) eqn:E.
  - rewrite Hs. reflexivity.
  - apply slt_false in E. lia.
Qed.

(** * The fragment on which the tagged order IS the order on the value

    [nnu x] - "not negative unsigned" - says x is not in area 3.  Equivalently (nnu_iff) x
    is Signed OR its value is non-negative, which is how the premise reads when stated
    about the module rather than about the ranking function.

    On that fragment area is decided by the SIGN of the value alone - 1 below zero, 2 at or
    above it, never 3 - so the lexicographic (area, i) order collapses to the order on i
    and ordinary integer reasoning becomes valid.  That is the only property the shift
    lemmas further down need, and it is strictly weaker than the [sgn] the arithmetic
    carries: Unsigned 0 is nnu and is not sgn.  sgn_lex and sgn_lexlt are derived from
    nnu_lex and nnu_lexlt at the bottom of the R4 section rather than proved a second time.

    WHAT NNU IS NOT: closed downward.  [add one] preserves it, [add minus_one] does not -
    Unsigned 0 is in area 2 and Unsigned (-1) is in area 3, the top one.  Every lemma below
    that shifts DOWN therefore hypothesises nnu of the shifted literal rather than of the
    original, and witness_the_shift_down_needs_more_than_nnu at the end of the file is the
    executed counterexample to the version that does not. *)

Definition nnu (x : IntLit.t) : Prop := IntLit.area x <> 3.

Lemma nnu_iff : forall x,
    nnu x <-> (x.(IntLit.t.signedness) = IntLit.Signed \/ 0 <= x.(IntLit.t.i)).
Proof.
  intros x. unfold nnu. split.
  - intros H. destruct (Z_lt_le_dec x.(IntLit.t.i) 0) as [Hx | Hx]; [ | right; exact Hx ].
    left. destruct x.(IntLit.t.signedness) eqn:S; [ reflexivity | ].
    exfalso. apply H. exact (area_neg_unsigned x Hx S).
  - intros [S | H].
    + destruct (Z_lt_le_dec x.(IntLit.t.i) 0) as [Hx | Hx].
      * rewrite (area_neg_signed x Hx S). discriminate.
      * rewrite (area_nonneg x Hx). discriminate.
    + rewrite (area_nonneg x H). discriminate.
Qed.

(** the only two areas an nnu literal can be in, together with the sign that selects it *)
Lemma nnu_area : forall x, nnu x ->
    (x.(IntLit.t.i) < 0 /\ IntLit.area x = 1) \/ (0 <= x.(IntLit.t.i) /\ IntLit.area x = 2).
Proof.
  intros x H. destruct (area_cases x) as [[A I] | [[A I] | [A I]]].
  - left; split; assumption.
  - right; split; assumption.
  - exfalso. exact (H A).
Qed.

Lemma nnu_lex : forall x y, nnu x -> nnu y ->
    (x.(IntLit.t.i) <= y.(IntLit.t.i) <-> lexle (irank x) (irank y)).
Proof.
  intros x y Hx Hy. unfold lexle, irank, fst, snd.
  destruct (nnu_area x Hx) as [[Ix Ax] | [Ix Ax]];
    destruct (nnu_area y Hy) as [[Iy Ay] | [Iy Ay]]; rewrite Ax, Ay; lia.
Qed.

Lemma nnu_lexlt : forall x y, nnu x -> nnu y ->
    (x.(IntLit.t.i) < y.(IntLit.t.i) <-> lexlt (irank x) (irank y)).
Proof.
  intros x y Hx Hy. unfold lexlt, irank, fst, snd.
  destruct (nnu_area x Hx) as [[Ix Ax] | [Ix Ax]];
    destruct (nnu_area y Hy) as [[Iy Ay] | [Iy Ay]]; rewrite Ax, Ay; lia.
Qed.

Lemma nnu_ile : forall x y, nnu x -> nnu y ->
    (ile x y <-> x.(IntLit.t.i) <= y.(IntLit.t.i)).
Proof. intros x y Hx Hy. rewrite ile_iff. symmetry. exact (nnu_lex x y Hx Hy). Qed.

Lemma nnu_ilt : forall x y, nnu x -> nnu y ->
    (ilt x y <-> x.(IntLit.t.i) < y.(IntLit.t.i)).
Proof. intros x y Hx Hy. rewrite ilt_iff. symmetry. exact (nnu_lexlt x y Hx Hy). Qed.

(** An nnu literal is below every literal that is NOT nnu, because area 3 is the top one.
    This is what makes the shift down sound on the UPPER side without any side condition:
    a bound that falls into area 3 admits everything the analyser can reach. *)
Lemma nnu_below_area3 : forall x b, nnu x -> ~ nnu b -> ilt x b.
Proof.
  intros x b Hx Hb. unfold nnu in Hb.
  destruct (Z.eq_dec (IntLit.area b) 3) as [A | A]; [ | exfalso; exact (Hb A) ].
  apply ilt_iff. left. unfold irank, fst. rewrite A.
  destruct (nnu_area x Hx) as [[_ Ax] | [_ Ax]]; rewrite Ax; lia.
Qed.

(** u < y implies u+1 <= y, in IntLit's order.  True in every area combination, including
    the ones where the shift crosses a boundary - there the hypothesis turns out to be
    unsatisfiable, which is what makes it go through. *)
Lemma add_one_le : forall u y, ilt u y -> ile (IntLit.add IntLit.one u) y.
Proof.
  intros u y H. apply ilt_iff in H. apply ile_iff.
  unfold lexle, lexlt, irank, fst, snd in *.
  pose proof (area_cases y) as Hy.
  pose proof (add_one_i u) as Ii. pose proof (add_one_sign u) as Is.
  destruct (Z_lt_le_dec u.(IntLit.t.i) 0) as [Hu | Hu].
  - destruct u.(IntLit.t.signedness) eqn:Su.
    + rewrite (area_neg_signed u Hu Su) in H.
      destruct (Z_lt_le_dec (1 + u.(IntLit.t.i)) 0) as [Hs | Hs].
      * rewrite (area_neg_signed _ ltac:(rewrite Ii; exact Hs)
                                   Is), Ii;
          destruct Hy as [[Ay Hiy] | [[Ay Hiy] | [Ay Hiy]]]; lia.
      * rewrite (area_nonneg _ ltac:(rewrite Ii; exact Hs)), Ii;
          destruct Hy as [[Ay Hiy] | [[Ay Hiy] | [Ay Hiy]]]; lia.
    + rewrite (area_neg_unsigned u Hu Su) in H.
      destruct (Z_lt_le_dec (1 + u.(IntLit.t.i)) 0) as [Hs | Hs].
      * rewrite (area_neg_unsigned _ ltac:(rewrite Ii; exact Hs)
                                     Is), Ii;
          destruct Hy as [[Ay Hiy] | [[Ay Hiy] | [Ay Hiy]]]; lia.
      * rewrite (area_nonneg _ ltac:(rewrite Ii; exact Hs)), Ii;
          destruct Hy as [[Ay Hiy] | [[Ay Hiy] | [Ay Hiy]]]; lia.
  - rewrite (area_nonneg u Hu) in H.
    assert (Hs : 0 <= 1 + u.(IntLit.t.i)) by lia.
    rewrite (area_nonneg _ ltac:(rewrite Ii; exact Hs)), Ii;
      destruct Hy as [[Ay Hiy] | [[Ay Hiy] | [Ay Hiy]]]; lia.
Qed.

(** * Concretisation

    gamma a x says the analyser's abstract value [a] admits the concrete literal [x], with
    the bounds compared by IntLit's own order. *)

Definition sat_lower (b : Bound.t) (x : IntLit.t) : Prop :=
  match b with
  | Bound.MinusInfinity => True
  | Bound.PlusInfinity => False
  | Bound.Int i => ile i x
  end.

Definition sat_upper (b : Bound.t) (x : IntLit.t) : Prop :=
  match b with
  | Bound.PlusInfinity => True
  | Bound.MinusInfinity => False
  | Bound.Int i => ile x i
  end.

Definition gamma (a : PulseCItv.t) (x : IntLit.t) : Prop :=
  match a with
  | Unsafe.Between l u => sat_lower l x /\ sat_upper u x
  | Unsafe.Outside i j => ilt x i \/ ilt j x
  end.

Definition refines (o : option PulseCItv.t) (x : IntLit.t) : Prop :=
  match o with
  | None => True
  | Some b => gamma b x
  end.

(** * Bound-level lemmas *)

Lemma sat_upper_mono : forall u x y, ile x y -> sat_upper u y -> sat_upper u x.
Proof.
  intros u x y Hxy H. destruct u as [i | |]; cbn in *;
    [ exact (ile_trans _ _ _ Hxy H) | exact H | exact I ].
Qed.

Lemma sat_lower_mono : forall l x y, ile x y -> sat_lower l x -> sat_lower l y.
Proof.
  intros l x y Hxy H. destruct l as [i | |]; cbn in *;
    [ exact (ile_trans _ _ _ H Hxy) | exact I | exact H ].
Qed.

(** Bound.min / Bound.max return one of their arguments, so they preserve whichever side
    already holds. *)
Lemma sat_upper_min : forall u1 u2 x,
    sat_upper u1 x -> sat_upper u2 x -> sat_upper (Bound.min u1 u2) x.
Proof. intros u1 u2 x H1 H2. unfold Bound.min. destruct (Bound.le u1 u2); assumption. Qed.

Lemma sat_lower_max : forall l1 l2 x,
    sat_lower l1 x -> sat_lower l2 x -> sat_lower (Bound.max l1 l2) x.
Proof. intros l1 l2 x H1 H2. unfold Bound.max. destruct (Bound.le l1 l2); assumption. Qed.

(** If the code decides b1 < b2 as bounds then nothing under b1 reaches anything over b2.
    Bound.lt is not a strict order - it answers true at both infinities - but those cases
    are vacuous because nothing is under MinusInfinity or over PlusInfinity. *)
Lemma bound_lt_sound : forall b1 b2 x y,
    Bound.lt b1 b2 = true -> sat_upper b1 x -> sat_lower b2 y -> ilt x y.
Proof.
  intros b1 b2 x y Hlt Hx Hy.
  destruct b1 as [i1 | |]; destruct b2 as [i2 | |];
    cbn in Hlt, Hx, Hy; try contradiction; try discriminate.
  (* Int i1 < Int i2, x <= i1, i2 <= y *)
  apply ile_ilt_trans with (y := i1); [ exact Hx | ].
  apply ilt_ile_trans with (y := i2); [ exact Hlt | exact Hy ].
Qed.

(** * R0: the Unsatisfiable case of abduce_le *)

Theorem abduce_le_unsat_sound :
  forall (a1 a2 : PulseCItv.t),
    PulseCItv.abduce_le a1 a2 = Unsatisfiable ->
    forall x y : IntLit.t, gamma a1 x -> gamma a2 y -> ~ ile x y.
Proof.
  intros a1 a2 Hunsat x y G1 G2 Hxy.
  destruct a1 as [lower1 upper1 | l1 u1];
    destruct a2 as [lower2 upper2 | l2 u2];
    unfold PulseCItv.abduce_le in Hunsat.
  - destruct G1 as [Gl1 Gu1]; destruct G2 as [Gl2 Gu2].
    (* matched on the SHAPE of the guard, not on its literal text, so that a mutant of
       abduce_le still reaches this point and fails for a real reason *)
    match type of Hunsat with
    | (if ?c then _ else _) = _ => destruct c eqn:E
    end; [ | discriminate ].
    apply Bool.orb_true_iff in E. destruct E as [E | E].
    + assert (Hup : sat_upper (Bound.min upper1 upper2) x).
      { apply sat_upper_min; [ exact Gu1 | exact (sat_upper_mono _ _ _ Hxy Gu2) ]. }
      pose proof (bound_lt_sound _ _ _ _ E Hup Gl1) as C. exact (ilt_irrefl _ C).
    + assert (Hlo : sat_lower (Bound.max lower1 lower2) y).
      { apply sat_lower_max; [ exact (sat_lower_mono _ _ _ Hxy Gl1) | exact Gl2 ]. }
      pose proof (bound_lt_sound _ _ _ _ E Gu2 Hlo) as C. exact (ilt_irrefl _ C).
  - destruct (Bound.lt lower1 (Bound.Int l2)); discriminate.
  - destruct (Bound.ge (Bound.Int u1) upper2); discriminate.
  - discriminate.
Qed.

(** * R1: the Satisfiable case of abduce_le loses no solution

    Three of the four operand shapes. The fourth, Outside on the left against Between on the
    right, is NOT sound over IntLit's real order, and is refuted immediately below rather
    than quietly left out. *)

(** [Bound.ge (Int u1) b] with y under b gives y <= u1: the guard of the Outside/Between
    arm, in the form the proof needs. *)
(** [geq] is the only comparison the ord tactic does not reduce on its own, because
    nothing used it before this arm. *)
Lemma geq_true_ile : forall a b, IntLit.geq a b = true -> ile b a.
Proof.
  intros a b H. unfold IntLit.geq in H. apply sge_true in H. apply ile_iff.
  destruct (compare_value_spec a b) as [[Hc Hlt] | [[Hc [Ha Hi]] | [Hc Hlt]]];
    rewrite Hc in H; unfold lexle, lexlt, irank, fst, snd in *; lia.
Qed.

Lemma ge_int_upper : forall u1 b y,
    Bound.ge (Bound.Int u1) b = true -> sat_upper b y -> ile y u1.
Proof.
  intros u1 b y H Hy. destruct b as [i | |]; cbn in H, Hy.
  - exact (ile_trans _ _ _ Hy (geq_true_ile _ _ H)).
  - contradiction.
  - discriminate.
Qed.

Theorem abduce_le_sat_sound :
  forall (a1 a2 : PulseCItv.t) (o1 o2 : option PulseCItv.t),
    PulseCItv.abduce_le a1 a2 = Satisfiable o1 o2 ->
    forall x y : IntLit.t, gamma a1 x -> gamma a2 y -> ile x y ->
      refines o1 x /\ refines o2 y.
Proof.
  intros a1 a2 o1 o2 Hsat x y G1 G2 Hxy.
  destruct a1 as [lower1 upper1 | l1 u1];
    destruct a2 as [lower2 upper2 | l2 u2];
    unfold PulseCItv.abduce_le in Hsat.
  - destruct G1 as [Gl1 Gu1]; destruct G2 as [Gl2 Gu2].
    match type of Hsat with
    | (if ?c then _ else _) = _ => destruct c eqn:E
    end; [ discriminate | ].
    inversion Hsat; subst; clear Hsat. cbn [between]. split; cbn [gamma].
    + split; [ exact Gl1 | ].
      apply sat_upper_min; [ exact Gu1 | exact (sat_upper_mono _ _ _ Hxy Gu2) ].
    + split; [ | exact Gu2 ].
      apply sat_lower_max; [ exact (sat_lower_mono _ _ _ Hxy Gl1) | exact Gl2 ].
  - (* Between / Outside *)
    destruct G1 as [Gl1 Gu1].
    destruct (Bound.lt lower1 (Bound.Int l2)) eqn:E.
    + inversion Hsat; subst. split; exact I.
    + inversion Hsat; subst; clear Hsat. split; [ exact I | ].
      cbn [between gamma sat_upper]. split; [ | exact I ].
      apply sat_lower_max; [ | exact (sat_lower_mono _ _ _ Hxy Gl1) ].
      (* the refinement's own lower bound, 1 + u2, must admit y.  G2 offers y < l2 or
         u2 < y, and the guard rules out the first: NOT (lower1 < l2) gives
         l2 <= lower1 <= x <= y. *)
      cbn [sat_lower].
      assert (Hnl : ~ ilt y l2).
      { destruct lower1 as [i | |]; cbn in E, Gl1.
        - intro K. apply ilt_false in E.
          assert (Hiy : ile i y) by exact (ile_trans _ _ _ Gl1 Hxy).
          ord.
        - discriminate.
        - contradiction. }
      destruct G2 as [G2 | G2]; [ contradiction | ].
      (* u2 < y, and the refinement's lower bound is exactly IntLit.add one u2 *)
      exact (add_one_le _ _ G2).
  - (* Outside / Between.  This arm used to be excluded by a hypothesis and refuted by a
       counterexample: it bounded x by l1+1, and adding to a literal joins the signedness
       tag, so for an unsigned l1 the sum lands in a lower area and stops being an upper
       bound.  The bound is now l1 itself, which needs no arithmetic and therefore cannot
       cross an area, so the arm holds for EVERY signedness and the hypothesis is gone. *)
    destruct G2 as [Gl2 Gu2].
    destruct (Bound.ge (Bound.Int u1) upper2) eqn:E.
    + inversion Hsat; subst; clear Hsat.
      split; [ | exact I ]. cbn [between gamma sat_lower]. split; [ exact I | ].
      apply sat_upper_min.
      * (* x <= l1.  Either x is below l1 already, or it is above u1 - and then
           u1 < x <= y <= upper2 <= u1 is impossible. *)
        cbn [sat_upper]. destruct G1 as [G1 | G1].
        -- exact (ilt_ile _ _ G1).
        -- exfalso. apply (ilt_irrefl u1).
           exact (ilt_ile_trans _ _ _ G1
                    (ile_trans _ _ _ Hxy (ge_int_upper _ _ _ E Gu2))).
      * exact (sat_upper_mono _ _ _ Hxy Gu2).
    + inversion Hsat; subst. split; exact I.
  - inversion Hsat; subst. split; exact I.
Qed.

(** The arm the theorem above excludes, refuted by computation on the translated code.

    The refinement abduce_le returns for the left operand is x <= min(l1+1, upper2).  Over Z
    that is merely imprecise - x < l1 gives x <= l1-1 <= l1+1.  Over IntLit's order it is
    unsound, because adding one can cross an area boundary DOWNWARDS: Unsigned(-1) is in
    area 3, the top area, and Unsigned(0) is in area 2, so l1+1 is BELOW l1 and the
    refinement cuts below a value that satisfies the constraint. *)

Definition um1 : IntLit.t := IntLit.t.Build IntLit.Unsigned (-1) IntLit.NotPointer.
Definition um2 : IntLit.t := IntLit.t.Build IntLit.Unsigned (-2) IntLit.NotPointer.

(** The case R1 REFUTED, now a proof, on the very literals that broke it.

    It used to read: abduce_le returns a refinement bounding x by l1+1, with l1 unsigned
    -1; adding one joins the tag and lands on unsigned 0, which sits in area 2 while l1
    sits in area 3, so the "upper bound" is below the value it is supposed to admit, and
    um2 - genuinely in Outside(um1,um1) and genuinely <= itself - falls outside the
    refinement. The arm now bounds x by l1 itself, so there is no arithmetic to cross an
    area, and the same instance goes through. Kept as a pinned regression check: if the
    arithmetic ever comes back, this fails. *)
Theorem abduce_le_sat_sound_on_the_formerly_refuted_case :
  forall o1 o2,
    PulseCItv.abduce_le (Unsafe.Outside um1 um1)
      (Unsafe.Between (Bound.Int um2) (Bound.Int um2)) = Satisfiable o1 o2 ->
    refines o1 um2 /\ refines o2 um2.
Proof.
  intros o1 o2 H.
  apply (abduce_le_sat_sound _ _ _ _ H um2 um2).
  - (* um2 is in Outside(um1,um1): strictly below um1 *)
    left; unfold ilt; vm_compute; reflexivity.
  - (* and in [um2,um2] *)
    split; unfold ile; vm_compute; reflexivity.
  - (* and um2 <= um2 *)
    unfold ile; vm_compute; reflexivity.
Qed.

Print Assumptions abduce_le_unsat_sound.
Print Assumptions abduce_le_sat_sound.

(** * Regression checks: the two witnesses that rejected the previous model

    upstream-triage executed these against the real OCaml and against the old
    [type t = int] translation, and they disagreed.  Under the faithful model they must
    agree, so they are kept here and run by the pipeline.

    Real OCaml (verify-fv-rocq-m1/signedness-witness/witness.log):
      IntLit.leq unsigned(-1) signed(0)                        = false
      abduce_le (equal_to unsigned(-1)) (equal_to signed(0))   = Unsatisfiable
      abduce_lt (equal_to signed(0)) (equal_to unsigned(-1))   = Satisfiable
    The erased Z model gave leq = true and the two answers the other way round. *)

Definition s0 : IntLit.t := IntLit.t.Build IntLit.Signed 0 IntLit.NotPointer.

Example witness_leq_unsigned_minus_one_vs_signed_zero :
  IntLit.leq um1 s0 = false.
Proof. vm_compute. reflexivity. Qed.

Example witness_abduce_le_unsigned_minus_one_signed_zero :
  PulseCItv.abduce_le (PulseCItv.equal_to um1) (PulseCItv.equal_to s0) = Unsatisfiable.
Proof. vm_compute. reflexivity. Qed.

Example witness_abduce_lt_signed_zero_unsigned_minus_one :
  PulseCItv.abduce_lt (PulseCItv.equal_to s0) (PulseCItv.equal_to um1) <> Unsatisfiable.
Proof. vm_compute. discriminate. Qed.

(** and the ordering fact underneath all three: a negative UNSIGNED literal sits above
    every non-negative one, which is what the plain Z order got wrong *)
Example witness_negative_unsigned_is_above_zero :
  IntLit.lt s0 um1 = true.
Proof. vm_compute. reflexivity. Qed.

Print Assumptions abduce_le_sat_sound_on_the_formerly_refuted_case.

(** * Equality is NOT structural, and lift1 keeps the tags

    infer/src/IR/IntLit.ml:172 is [let equal i1 i2 = eq i1 i2], which SHADOWS the [equal]
    that [@@deriving equal] generates on the record.  So the exported equality is the
    area-based one: it ignores pointerness outright, and it identifies signed(0) with
    unsigned(0) because both are in area 2 with the same value.  An earlier version of this
    directory hand-wrote a structural equality here and claimed it matched; these checks
    exist so that cannot happen silently again.

    Real OCaml:
      IntLit.equal signed(0) unsigned(0)                  = true
      IntLit.equal signed(5)@Pointer signed(5)@NotPointer = true
      IntLit.equal signed(-1) unsigned(-1)                = false   (areas 1 and 3) *)

Definition u0  : IntLit.t := IntLit.t.Build IntLit.Unsigned 0 IntLit.NotPointer.
Definition sm1 : IntLit.t := IntLit.t.Build IntLit.Signed (-1) IntLit.NotPointer.
Definition p5  : IntLit.t := IntLit.t.Build IntLit.Signed 5 IntLit.Pointer.
Definition n5  : IntLit.t := IntLit.t.Build IntLit.Signed 5 IntLit.NotPointer.

Example witness_equal_identifies_signed_and_unsigned_zero :
  IntLit.equal s0 u0 = true.
Proof. vm_compute. reflexivity. Qed.

Example witness_equal_ignores_pointerness :
  IntLit.equal p5 n5 = true.
Proof. vm_compute. reflexivity. Qed.

(** but it does distinguish the two negative tags, because they land in different areas *)
Example witness_equal_distinguishes_negative_tags :
  IntLit.equal sm1 um1 = false.
Proof. vm_compute. reflexivity. Qed.

(** lift1 is [{x with i = f x.i}]: it must carry BOTH tags through unchanged.  Negating an
    unsigned literal keeps it unsigned, and negating a pointer keeps it a pointer.  Without
    these, a lift1 that rebuilds a default Signed/NotPointer record goes unnoticed. *)
Example witness_lift1_keeps_signedness :
  (IntLit.neg um1).(IntLit.t.signedness) = IntLit.Unsigned.
Proof. vm_compute. reflexivity. Qed.

Example witness_lift1_keeps_pointerness :
  (IntLit.neg p5).(IntLit.t.pointerness) = IntLit.Pointer.
Proof. vm_compute. reflexivity. Qed.

(** * R4: the arithmetic - add, minus, unop, binop

    The statement R4 was planned with is

      binop op a1 a2 = Some r -> gamma a1 x -> gamma a2 y -> gamma r (x op y)

    and it is FALSE on this module. IntLit's order is lexicographic on (area, i), and
    lift1/lift2 join the signedness tag, so an operation can move a literal between AREAS
    and thereby reorder it without the arithmetic having moved it. Two consequences, both
    refuted by computation at the end of this section:

      add is not monotone     Signed 1 <= Unsigned (-1), yet
                              Signed 1 + Unsigned 2 = Unsigned 3 is ABOVE
                              Unsigned (-1) + Unsigned 2 = Unsigned 1
      neg is not antitone     neg (Signed 1) = Signed (-1) is in area 1, while
                              neg (Unsigned (-1)) = Unsigned 1 is in area 2

    So the arithmetic is proved on the fragment where the order behaves: the SIGNED
    literals. That is not an arbitrary restriction. On Signed values area is 1 exactly
    when i < 0 and 2 otherwise, so the lexicographic order COINCIDES with the order on i
    (sgn_ile below), and ordinary interval reasoning becomes valid. The fragment is closed
    under the operations, because join_signedness Signed Signed = Signed. *)

Definition sgn (x : IntLit.t) : Prop := x.(IntLit.t.signedness) = IntLit.Signed.

(** Every Signed literal is in the nnu fragment, and the inclusion is STRICT: Unsigned 0
    is nnu and is not sgn.  So the four bridge lemmas below are instances of the nnu ones
    proved with the areas, not a second development.  They kept their names and statements
    because the arithmetic proofs further down are stated in terms of them. *)
Lemma sgn_nnu : forall x, sgn x -> nnu x.
Proof. intros x Sx. apply nnu_iff. left. exact Sx. Qed.

(** On Signed literals the tagged order and the order on the value agree. *)
Lemma sgn_lex : forall x y, sgn x -> sgn y ->
    (x.(IntLit.t.i) <= y.(IntLit.t.i) <-> lexle (irank x) (irank y)).
Proof. intros x y Sx Sy. exact (nnu_lex x y (sgn_nnu x Sx) (sgn_nnu y Sy)). Qed.

Lemma sgn_lexlt : forall x y, sgn x -> sgn y ->
    (x.(IntLit.t.i) < y.(IntLit.t.i) <-> lexlt (irank x) (irank y)).
Proof. intros x y Sx Sy. exact (nnu_lexlt x y (sgn_nnu x Sx) (sgn_nnu y Sy)). Qed.

Lemma sgn_ile : forall x y, sgn x -> sgn y ->
    (ile x y <-> x.(IntLit.t.i) <= y.(IntLit.t.i)).
Proof. intros x y Sx Sy. exact (nnu_ile x y (sgn_nnu x Sx) (sgn_nnu y Sy)). Qed.

Lemma sgn_ilt : forall x y, sgn x -> sgn y ->
    (ilt x y <-> x.(IntLit.t.i) < y.(IntLit.t.i)).
Proof. intros x y Sx Sy. exact (nnu_ilt x y (sgn_nnu x Sx) (sgn_nnu y Sy)). Qed.

(** The fragment is closed, and the value components are the obvious ones. *)
Lemma add_i : forall x y,
    (IntLit.add x y).(IntLit.t.i) = x.(IntLit.t.i) + y.(IntLit.t.i).
Proof. intros x y. reflexivity. Qed.

Lemma neg_i : forall x, (IntLit.neg x).(IntLit.t.i) = - x.(IntLit.t.i).
Proof. intros x. reflexivity. Qed.

Lemma sgn_add : forall x y, sgn x -> sgn y -> sgn (IntLit.add x y).
Proof.
  intros x y Sx Sy. unfold sgn, IntLit.add, IntLit.lift2, IntLit.join_signedness. cbn.
  rewrite Sx, Sy. reflexivity.
Qed.

Lemma sgn_neg : forall x, sgn x -> sgn (IntLit.neg x).
Proof. intros x Sx. exact Sx. Qed.

(** Subtraction is addition of the negation, tags and all. Needed because binop MinusA
    is implemented as [minus a_rhs >>= add a_lhs] while the concrete operation is [sub].
    This one holds with no signedness hypothesis: both sides join the same two tags. *)
Lemma sub_is_add_neg : forall x y, IntLit.sub x y = IntLit.add x (IntLit.neg y).
Proof.
  intros x y. unfold IntLit.sub, IntLit.add, IntLit.neg, IntLit.lift1, IntLit.lift2,
    IntLit.t.with_i.
  cbn. f_equal; try reflexivity; lia.
Qed.

(** ** Lifting the fragment to intervals

    [sgn_itv] says every literal an interval mentions is Signed.  The infinities carry no
    literal, so they are vacuously in the fragment. *)

Definition sgn_bound (b : Bound.t) : Prop :=
  match b with
  | Bound.Int i => sgn i
  | Bound.MinusInfinity | Bound.PlusInfinity => True
  end.

Definition sgn_itv (a : PulseCItv.t) : Prop :=
  match a with
  | Unsafe.Between l u => sgn_bound l /\ sgn_bound u
  | Unsafe.Outside i j => sgn i /\ sgn j
  end.

(** Bound.add has an arm that the generated file renders as the support library's [axiom]
    (the real code dies there).  It is reached only by MinusInfinity with PlusInfinity, and
    the concretisation rules that out on both sides: [sat_lower PlusInfinity] and
    [sat_upper MinusInfinity] are both False, so an inhabited interval never presents the
    offending pair.  That is why none of the theorems below depends on an axiom. *)

Lemma sat_lower_add : forall l1 l2 x y,
    sgn_bound l1 -> sgn_bound l2 -> sgn x -> sgn y ->
    sat_lower l1 x -> sat_lower l2 y ->
    sat_lower (Bound.add l1 l2) (IntLit.add x y).
Proof.
  intros l1 l2 x y S1 S2 Sx Sy H1 H2.
  destruct l1 as [i1 | |]; destruct l2 as [i2 | |]; cbn in *; try exact I;
    try contradiction.
  apply (sgn_ile _ _ (sgn_add i1 i2 S1 S2) (sgn_add x y Sx Sy)).
  rewrite !add_i.
  apply (sgn_ile _ _ S1 Sx) in H1. apply (sgn_ile _ _ S2 Sy) in H2. lia.
Qed.

Lemma sat_upper_add : forall u1 u2 x y,
    sgn_bound u1 -> sgn_bound u2 -> sgn x -> sgn y ->
    sat_upper u1 x -> sat_upper u2 y ->
    sat_upper (Bound.add u1 u2) (IntLit.add x y).
Proof.
  intros u1 u2 x y S1 S2 Sx Sy H1 H2.
  destruct u1 as [i1 | |]; destruct u2 as [i2 | |]; cbn in *; try exact I;
    try contradiction.
  apply (sgn_ile _ _ (sgn_add x y Sx Sy) (sgn_add i1 i2 S1 S2)).
  rewrite !add_i.
  apply (sgn_ile _ _ Sx S1) in H1. apply (sgn_ile _ _ Sy S2) in H2. lia.
Qed.

(** * add *)
Theorem add_sound : forall a1 a2 r x y,
    sgn_itv a1 -> sgn_itv a2 -> sgn x -> sgn y ->
    PulseCItv.add a1 a2 = Some r ->
    gamma a1 x -> gamma a2 y ->
    gamma r (IntLit.add x y).
Proof.
  intros a1 a2 r x y S1 S2 Sx Sy Hr G1 G2.
  destruct a1 as [l1 u1 | p1 q1]; destruct a2 as [l2 u2 | p2 q2];
    cbn in Hr; try discriminate.
  injection Hr as <-. destruct S1 as [Sl1 Su1]. destruct S2 as [Sl2 Su2].
  destruct G1 as [Gl1 Gu1]. destruct G2 as [Gl2 Gu2].
  split.
  - exact (sat_lower_add _ _ _ _ Sl1 Sl2 Sx Sy Gl1 Gl2).
  - exact (sat_upper_add _ _ _ _ Su1 Su2 Sx Sy Gu1 Gu2).
Qed.

(** * minus

    Negation must be ANTITONE for the Between arm, because the constructor swaps the
    endpoints.  It is, on Signed literals; see the refutation at the end for why the
    hypothesis cannot be dropped. *)

Lemma sat_lower_minus : forall u x, sgn_bound u -> sgn x ->
    sat_upper u x -> sat_lower (Bound.minus u) (IntLit.neg x).
Proof.
  intros u x Su Sx H. destruct u as [i | |]; cbn in *; try exact I; try contradiction.
  apply (sgn_ile _ _ (sgn_neg i Su) (sgn_neg x Sx)).
  rewrite !neg_i. apply (sgn_ile _ _ Sx Su) in H. lia.
Qed.

Lemma sat_upper_minus : forall l x, sgn_bound l -> sgn x ->
    sat_lower l x -> sat_upper (Bound.minus l) (IntLit.neg x).
Proof.
  intros l x Sl Sx H. destruct l as [i | |]; cbn in *; try exact I; try contradiction.
  apply (sgn_ile _ _ (sgn_neg x Sx) (sgn_neg i Sl)).
  rewrite !neg_i. apply (sgn_ile _ _ Sl Sx) in H. lia.
Qed.

Theorem minus_sound : forall a r x,
    sgn_itv a -> sgn x ->
    PulseCItv.minus a = Some r -> gamma a x -> gamma r (IntLit.neg x).
Proof.
  intros a r x Sa Sx Hr G. destruct a as [l u | p q]; cbn in Hr; injection Hr as <-.
  - destruct Sa as [Sl Su]. destruct G as [Gl Gu]. split.
    + exact (sat_lower_minus _ _ Su Sx Gu).
    + exact (sat_upper_minus _ _ Sl Sx Gl).
  - (* the Outside arm: x < p or q < x, negated, swaps which disjunct holds *)
    destruct Sa as [Sp Sq]. cbn in *. destruct G as [G | G].
    + right. apply (sgn_ilt _ _ (sgn_neg p Sp) (sgn_neg x Sx)).
      rewrite !neg_i. apply (sgn_ilt _ _ Sx Sp) in G. lia.
    + left. apply (sgn_ilt _ _ (sgn_neg x Sx) (sgn_neg q Sq)).
      rewrite !neg_i. apply (sgn_ilt _ _ Sq Sx) in G. lia.
Qed.

Lemma sgn_itv_minus : forall a r, sgn_itv a -> PulseCItv.minus a = Some r -> sgn_itv r.
Proof.
  intros a r Sa Hr. destruct a as [l u | p q]; cbn in Hr; injection Hr as <-.
  - destruct Sa as [Sl Su]. split.
    + destruct u as [i | |]; cbn in Su |- *;
        [ exact (sgn_neg i Su) | exact I | exact I ].
    + destruct l as [i | |]; cbn in Sl |- *;
        [ exact (sgn_neg i Sl) | exact I | exact I ].
  - destruct Sa as [Sp Sq]. split; [ exact (sgn_neg q Sq) | exact (sgn_neg p Sp) ].
Qed.

(** * unop

    Only Neg is interpreted; the other two decline, so there is nothing to be unsound
    about.  Stated rather than skipped: "returns None" is most of this function. *)

Theorem unop_neg_sound : forall a r x,
    sgn_itv a -> sgn x ->
    PulseCItv.unop Unop.Neg a = Some r -> gamma a x -> gamma r (IntLit.neg x).
Proof. intros a r x Sa Sx Hr G. exact (minus_sound a r x Sa Sx Hr G). Qed.

Lemma unop_bnot_is_none : forall a, PulseCItv.unop Unop.BNot a = None.
Proof. reflexivity. Qed.

Lemma unop_lnot_is_none : forall a, PulseCItv.unop Unop.LNot a = None.
Proof. reflexivity. Qed.

(** * binop

    PlusA is add and MinusA is [minus a_rhs >>= add a_lhs].  The concrete operation for
    MinusA is IntLit.sub, which is why sub_is_add_neg is needed: the implementation goes
    round through the negation and the statement must still be about subtraction. *)

Theorem binop_PlusA_sound : forall k a1 a2 r x y,
    sgn_itv a1 -> sgn_itv a2 -> sgn x -> sgn y ->
    PulseCItv.binop (Binop.PlusA k) a1 a2 = Some r ->
    gamma a1 x -> gamma a2 y -> gamma r (IntLit.add x y).
Proof.
  intros k a1 a2 r x y S1 S2 Sx Sy Hr G1 G2.
  exact (add_sound a1 a2 r x y S1 S2 Sx Sy Hr G1 G2).
Qed.

Theorem binop_MinusA_sound : forall k a1 a2 r x y,
    sgn_itv a1 -> sgn_itv a2 -> sgn x -> sgn y ->
    PulseCItv.binop (Binop.MinusA k) a1 a2 = Some r ->
    gamma a1 x -> gamma a2 y -> gamma r (IntLit.sub x y).
Proof.
  intros k a1 a2 r x y S1 S2 Sx Sy Hr G1 G2.
  cbn in Hr.
  destruct (PulseCItv.minus a2) as [m |] eqn:Hm; cbn in Hr; [| discriminate].
  rewrite sub_is_add_neg.
  apply (add_sound a1 m r x (IntLit.neg y) S1
           (sgn_itv_minus a2 m S2 Hm) Sx (sgn_neg y Sy) Hr G1).
  exact (minus_sound a2 m y S2 Sy Hm G2).
Qed.

(** These eighteen operators decline. DivI and Mod are implemented but not listed here. *)
Lemma binop_unsupported_is_none : forall a1 a2,
    PulseCItv.binop Binop.PlusPI a1 a2 = None
    /\ PulseCItv.binop Binop.MinusPI a1 a2 = None
    /\ PulseCItv.binop Binop.MinusPP a1 a2 = None
    /\ (forall k, PulseCItv.binop (Binop.Mult k) a1 a2 = None)
    /\ PulseCItv.binop Binop.DivF a1 a2 = None
    /\ PulseCItv.binop Binop.Shiftlt a1 a2 = None
    /\ PulseCItv.binop Binop.Shiftrt a1 a2 = None
    /\ PulseCItv.binop Binop.Lt a1 a2 = None
    /\ PulseCItv.binop Binop.Gt a1 a2 = None
    /\ PulseCItv.binop Binop.Le a1 a2 = None
    /\ PulseCItv.binop Binop.Ge a1 a2 = None
    /\ PulseCItv.binop Binop.Eq a1 a2 = None
    /\ PulseCItv.binop Binop.Ne a1 a2 = None
    /\ PulseCItv.binop Binop.BAnd a1 a2 = None
    /\ PulseCItv.binop Binop.BXor a1 a2 = None
    /\ PulseCItv.binop Binop.BOr a1 a2 = None
    /\ PulseCItv.binop Binop.LAnd a1 a2 = None
    /\ PulseCItv.binop Binop.LOr a1 a2 = None.
Proof. intros a1 a2. repeat split; reflexivity. Qed.

(** * Why the Signed hypothesis is the right one, and why it is not a restriction

    Every theorem above carries [sgn]. Two separate facts justify that.

    FIRST, it cannot be dropped. The order is lexicographic on (area, i) and lift1/lift2
    join the signedness tag, so an operation can move a literal between AREAS and reorder
    it without the arithmetic having moved it. The four results below are computations,
    not assumptions: add is not monotone, neg is not antitone, and each failure escapes to
    the interval level, where a value really in the operand has its real image outside the
    returned interval.

    SECOND, it costs nothing, because every IntLit a Pulse analysis can build is Signed.
    That is an invariant of the OCaml module rather than something provable here, and it
    was established by inspection of infer/src/IR/IntLit.ml and its interface:

      - the record is constructed in exactly two places, [of_z] and [null], and both write
        [signedness= Signed];
      - every other producer goes through [lift1], which preserves the tag, or [lift2],
        which joins two of them, and Signed joined with Signed is Signed;
      - [type t] is abstract in IntLit.mli, which never mentions Unsigned or the field, so
        no caller can build one;
      - the derived [t_of_sexp], the one remaining way in, has no callers.

    The CLOSURE half of that argument is machine-checked here - sgn_add, sgn_neg and
    sgn_itv_minus are exactly "the operations stay in the fragment". The base case, that
    every constructor yields Signed, is the inspection above and is not formalised: this
    development sees the translated PulseCItv, not the whole of IntLit.ml.

    So the refutations below describe values the analyser cannot currently build. They are
    kept as REGRESSION CHECKS on that invariant: the day something starts producing an
    Unsigned literal - a new constructor, an exposed field, a deserialiser with a caller -
    these four stop being hypothetical and the theorems above stop covering the reachable
    inputs. *)

Definition s1 : IntLit.t := IntLit.t.Build IntLit.Signed 1 IntLit.NotPointer.
Definition u2 : IntLit.t := IntLit.t.Build IntLit.Unsigned 2 IntLit.NotPointer.

(** The interval is well formed: its lower bound really is below its upper bound. *)
Example mixed_interval_is_well_formed : IntLit.leq s1 um1 = true.
Proof. reflexivity. Qed.

Definition mixed_a1 : PulseCItv.t := Unsafe.Between (Bound.Int s1) (Bound.Int um1).
Definition mixed_a2 : PulseCItv.t := Unsafe.Between (Bound.Int u2) (Bound.Int u2).

Theorem add_is_not_monotone_off_the_signed_fragment :
  IntLit.leq s1 um1 = true
  /\ IntLit.leq (IntLit.add s1 u2) (IntLit.add um1 u2) = false.
Proof. split; reflexivity. Qed.

Theorem neg_is_not_antitone_off_the_signed_fragment :
  IntLit.leq s1 um1 = true
  /\ IntLit.leq (IntLit.neg um1) (IntLit.neg s1) = false.
Proof. split; reflexivity. Qed.

(** Lifted: a value in the operand whose image is outside the result. *)
Theorem add_is_unsound_off_the_signed_fragment :
  gamma mixed_a1 um1 /\ gamma mixed_a2 u2 /\
  match PulseCItv.add mixed_a1 mixed_a2 with
  | Some r => gamma r (IntLit.add um1 u2) -> False
  | None => False
  end.
Proof.
  repeat split; unfold gamma, sat_lower, sat_upper, ile; cbn; try reflexivity.
  intros [H _]; discriminate.
Qed.

Theorem minus_is_unsound_off_the_signed_fragment :
  gamma mixed_a1 um1 /\
  match PulseCItv.minus mixed_a1 with
  | Some r => gamma r (IntLit.neg um1) -> False
  | None => False
  end.
Proof.
  repeat split; unfold gamma, sat_lower, sat_upper, ile; cbn; try reflexivity.
  intros [_ H]; discriminate.
Qed.

(** The arm of Bound.add that the real code dies in is rendered as a concrete bound (see
    strip.py). That choice is arbitrary, so it had better be unreachable - otherwise a
    theorem could be proved THROUGH it. It is: the arm is selected only by MinusInfinity
    together with PlusInfinity, and whichever way round they come, one of them contradicts
    the concretisation, because [sat_lower PlusInfinity] and [sat_upper MinusInfinity] are
    both False. Stated for both uses of Bound.add, the lower pair and the upper pair. *)
Theorem bound_add_die_arm_is_unreachable : forall b1 b2 x y,
    ((sat_lower b1 x /\ sat_lower b2 y) \/ (sat_upper b1 x /\ sat_upper b2 y)) ->
    ~ ((b1 = Bound.MinusInfinity /\ b2 = Bound.PlusInfinity)
       \/ (b1 = Bound.PlusInfinity /\ b2 = Bound.MinusInfinity)).
Proof.
  intros b1 b2 x y H C.
  destruct C as [[-> ->] | [-> ->]]; destruct H as [[H1 H2] | [H1 H2]]; cbn in *;
    contradiction.
Qed.

(** * The ported fragment: abduce_lt, the flip, and the dispatch

    Everything from here down replaces what verification/citv/lean proved against a
    hand-written model in which IntLit.t was plain Z.  The Lean names are given against
    each statement so the correspondence can be checked rather than taken on trust; the
    mapping table is in README.md.

    Three things are different, and all three are in the direction of saying more:

      - these are about the rocq-of-ocaml translation of the real PulseCItv.ml, which
        drift.sh regenerates byte-identically from the pinned source, not about a copy;
      - the concrete values are IntLit.t under the module's own (area, i) order, not Z;
      - where Lean restricted itself to two Between operands, the statements below quantify
        over PulseCItv.t, so Outside operands are covered rather than excluded.

    What they carry that Lean did not need is a hypothesis.  Over Z, adding one is
    monotone; over the tagged order it is not, because the shift can move a literal between
    AREAS.  [nnu] is the exact fragment where it is monotone, and the premise that every
    IntLit a Pulse analysis can build is in that fragment is pinned OUTSIDE Rocq, by
    intlit-signed-invariant.sh, for the reason given at signed_literals_are_closed. *)

(** ** Shifting a literal by one, in components *)

Lemma add_signedness : forall x k,
    (IntLit.add x k).(IntLit.t.signedness)
    = IntLit.join_signedness x.(IntLit.t.signedness) k.(IntLit.t.signedness).
Proof. reflexivity. Qed.

Lemma add_one_components : forall x,
    (IntLit.add x IntLit.one).(IntLit.t.signedness) = x.(IntLit.t.signedness)
    /\ (IntLit.add x IntLit.one).(IntLit.t.i) = x.(IntLit.t.i) + 1.
Proof.
  intros x. split; [ | reflexivity ].
  rewrite add_signedness. destruct x.(IntLit.t.signedness); reflexivity.
Qed.

Lemma add_minus_one_components : forall x,
    (IntLit.add x IntLit.minus_one).(IntLit.t.signedness) = x.(IntLit.t.signedness)
    /\ (IntLit.add x IntLit.minus_one).(IntLit.t.i) = x.(IntLit.t.i) - 1.
Proof.
  intros x. split.
  - rewrite add_signedness. destruct x.(IntLit.t.signedness); reflexivity.
  - rewrite add_i. cbn. lia.
Qed.

(** add_int reaches IntLit.add with its arguments the other way round in the Outside arm
    than in the Between arm, so the two have to be known equal. *)
Lemma add_comm : forall x y, IntLit.add x y = IntLit.add y x.
Proof.
  intros x y. unfold IntLit.add, IntLit.lift2.
  destruct x as [sx ix px]; destruct y as [sy iy py]; cbn. f_equal.
  - destruct sx; destruct sy; reflexivity.
  - lia.
  - destruct px; destruct py; reflexivity.
Qed.

(** The shift up and back is the IDENTITY, in all three fields - the value returns, the
    signedness join with two Signed operands is a no-op, and so is the pointerness join
    with two NotPointer ones.  This is what makes the left refinement of abduce_lt come
    back as the caller's own bound rather than as something computed, and it is why the
    sat half below needs no hypothesis about the bound the code produced. *)
Lemma add_one_then_minus_one : forall x,
    IntLit.add (IntLit.add x IntLit.one) IntLit.minus_one = x.
Proof.
  intros x. unfold IntLit.add, IntLit.lift2.
  destruct x as [s i p]; cbn. f_equal.
  - destruct s; reflexivity.
  - lia.
  - destruct p; reflexivity.
Qed.

Lemma bound_add_int_round_trip : forall b,
    Bound.add_int (Bound.add_int b IntLit.one) IntLit.minus_one = b.
Proof.
  intros b. destruct b as [i | |]; cbn; [ | reflexivity | reflexivity ].
  f_equal. exact (add_one_then_minus_one i).
Qed.

(** ** The fragment is closed under the shift up, and only under that one *)

Lemma nnu_add_one : forall x, nnu x -> nnu (IntLit.add x IntLit.one).
Proof.
  intros x H. apply nnu_iff in H. apply nnu_iff.
  destruct (add_one_components x) as [Hs Hi]. rewrite Hs, Hi.
  destruct H as [S | I]; [ left; exact S | right; lia ].
Qed.

(** The converse direction for the shift DOWN: nnu after the shift gives nnu before it.
    There is no lemma the other way round, and there cannot be - see
    witness_the_shift_down_needs_more_than_nnu. *)
Lemma nnu_of_shift_down : forall x, nnu (IntLit.add x IntLit.minus_one) -> nnu x.
Proof.
  intros x H. apply nnu_iff in H. apply nnu_iff.
  destruct (add_minus_one_components x) as [Hs Hi]. rewrite Hs, Hi in H.
  destruct H as [S | I]; [ left; exact S | right; lia ].
Qed.

Lemma ile_shift_up : forall a b, nnu a -> nnu b ->
    (ile (IntLit.add a IntLit.one) (IntLit.add b IntLit.one) <-> ile a b).
Proof.
  intros a b Ha Hb.
  rewrite (nnu_ile _ _ (nnu_add_one a Ha) (nnu_add_one b Hb)), (nnu_ile _ _ Ha Hb).
  destruct (add_one_components a) as [_ Ha1]. destruct (add_one_components b) as [_ Hb1].
  rewrite Ha1, Hb1. lia.
Qed.

Lemma ilt_shift_up : forall a b, nnu a -> nnu b ->
    (ilt (IntLit.add a IntLit.one) (IntLit.add b IntLit.one) <-> ilt a b).
Proof.
  intros a b Ha Hb.
  rewrite (nnu_ilt _ _ (nnu_add_one a Ha) (nnu_add_one b Hb)), (nnu_ilt _ _ Ha Hb).
  destruct (add_one_components a) as [_ Ha1]. destruct (add_one_components b) as [_ Hb1].
  rewrite Ha1, Hb1. lia.
Qed.

(** ** The fragment, lifted to bounds and to abstract values *)

Definition nnu_bound (b : Bound.t) : Prop :=
  match b with
  | Bound.Int i => nnu i
  | Bound.MinusInfinity | Bound.PlusInfinity => True
  end.

Definition nnu_itv (a : PulseCItv.t) : Prop :=
  match a with
  | Unsafe.Between l u => nnu_bound l /\ nnu_bound u
  | Unsafe.Outside i j => nnu i /\ nnu j
  end.

(** ** L9-L12: a bound shifted by one, against a value shifted with it

    Lean: Bound.allowsBelow_succ, allowsAbove_succ, allowsBelow_pred, allowsAbove_pred
    (Comparison.lean:125, 129, 133, 137), there with no hypothesis at all because there
    the values were Z.

    The two shifts DOWN hypothesise nnu of the ALREADY SHIFTED bound.  That is the weakest
    true precondition, not a convenience: nnu of the unshifted bound does not imply it
    (Unsigned 0 is in area 2, Unsigned (-1) is in area 3), and nnu_of_shift_down shows it
    implies the unshifted one, so nothing is lost by stating it this way round. *)

Theorem sat_lower_add_one_iff : forall l x, nnu_bound l -> nnu x ->
    (sat_lower (Bound.add_int l IntLit.one) (IntLit.add x IntLit.one) <-> sat_lower l x).
Proof.
  intros l x Hl Hx. destruct l as [i | |]; cbn in Hl |- *; [ | tauto | tauto ].
  exact (ile_shift_up i x Hl Hx).
Qed.

Theorem sat_upper_add_one_iff : forall u x, nnu_bound u -> nnu x ->
    (sat_upper (Bound.add_int u IntLit.one) (IntLit.add x IntLit.one) <-> sat_upper u x).
Proof.
  intros u x Hu Hx. destruct u as [i | |]; cbn in Hu |- *; [ | tauto | tauto ].
  exact (ile_shift_up x i Hx Hu).
Qed.

Theorem sat_lower_add_minus_one_iff : forall l x,
    nnu_bound (Bound.add_int l IntLit.minus_one) -> nnu x ->
    (sat_lower (Bound.add_int l IntLit.minus_one) x
     <-> sat_lower l (IntLit.add x IntLit.one)).
Proof.
  intros l x Hl Hx. destruct l as [i | |]; cbn in Hl |- *; [ | tauto | tauto ].
  pose proof (nnu_of_shift_down i Hl) as Hi.
  rewrite (nnu_ile _ _ Hl Hx), (nnu_ile _ _ Hi (nnu_add_one x Hx)).
  destruct (add_minus_one_components i) as [_ Hi1].
  destruct (add_one_components x) as [_ Hx1]. rewrite Hi1, Hx1. lia.
Qed.

Theorem sat_upper_add_minus_one_iff : forall u x,
    nnu_bound (Bound.add_int u IntLit.minus_one) -> nnu x ->
    (sat_upper (Bound.add_int u IntLit.minus_one) x
     <-> sat_upper u (IntLit.add x IntLit.one)).
Proof.
  intros u x Hu Hx. destruct u as [i | |]; cbn in Hu |- *; [ | tauto | tauto ].
  pose proof (nnu_of_shift_down i Hu) as Hi.
  rewrite (nnu_ile _ _ Hx Hu), (nnu_ile _ _ (nnu_add_one x Hx) Hi).
  destruct (add_minus_one_components i) as [_ Hi1].
  destruct (add_one_components x) as [_ Hx1]. rewrite Hi1, Hx1. lia.
Qed.

(** ** L13: membership of an interval shifted by one

    Lean: memBetween_succ (Comparison.lean:226). *)
Theorem gamma_between_add_one_iff : forall l u x,
    nnu_bound l -> nnu_bound u -> nnu x ->
    (gamma (Unsafe.Between (Bound.add_int l IntLit.one) (Bound.add_int u IntLit.one))
           (IntLit.add x IntLit.one)
     <-> gamma (Unsafe.Between l u) x).
Proof.
  intros l u x Hl Hu Hx. cbn [gamma].
  rewrite (sat_lower_add_one_iff l x Hl Hx), (sat_upper_add_one_iff u x Hu Hx).
  reflexivity.
Qed.

(** The same fact at the level of the whole abstract value, Outside arm included.  Only
    the direction abduce_lt uses is stated for Outside, because that is the only one the
    proof needs and an iff there would carry a hypothesis nothing can discharge. *)
Lemma gamma_add_one : forall a x, nnu_itv a -> nnu x ->
    gamma a x -> gamma (PulseCItv.add_int a IntLit.one) (IntLit.add x IntLit.one).
Proof.
  intros a x Na Nx G. destruct a as [l u | p q]; cbn [PulseCItv.add_int].
  - destruct Na as [Nl Nu]. cbn [between].
    apply (gamma_between_add_one_iff l u x Nl Nu Nx). exact G.
  - destruct Na as [Np Nq]. cbn [outside gamma] in G |- *.
    rewrite (add_comm IntLit.one p), (add_comm IntLit.one q).
    destruct G as [G | G].
    + left. apply (ilt_shift_up x p Nx Np). exact G.
    + right. apply (ilt_shift_up q x Nq Nx). exact G.
Qed.

(** The shift DOWN on the upper side needs no side condition at all, and the reason is
    worth stating: if the shifted bound falls into area 3 it is ABOVE every literal the
    analyser can reach, so it admits x for free.  The lower side has no such escape -
    an area-3 LOWER bound admits nothing - which is why abduce_lt's sat half goes through
    the structural lemma below instead of hypothesising about a computed bound. *)
Lemma sat_upper_shift_down : forall u x, nnu x ->
    sat_upper u (IntLit.add x IntLit.one) ->
    sat_upper (Bound.add_int u IntLit.minus_one) x.
Proof.
  intros u x Nx H. destruct u as [i | |]; cbn in H |- *; [ | exact H | exact I ].
  destruct (Z.eq_dec (IntLit.area (IntLit.add i IntLit.minus_one)) 3) as [A | A].
  - apply ilt_ile. apply (nnu_below_area3 x _ Nx). intro C. exact (C A).
  - apply (sat_upper_add_minus_one_iff (Bound.Int i) x A Nx). exact H.
Qed.

(** ** Where abduce_le's left refinement comes from

    The left refinement is never SOMETHING COMPUTED out of the right operand's bounds on
    its lower side: in the only two arms that return [Some] on the left it is either
    MinusInfinity or the left operand's own lower bound, unchanged.  abduce_lt shifts that
    refinement back down by one, and this is what makes the shift exact rather than
    hypothetical - after the round trip the literal is the caller's, not one the code
    built by adding. *)
Lemma abduce_le_lhs_shape : forall a1 a2 b o2,
    PulseCItv.abduce_le a1 a2 = Satisfiable (Some b) o2 ->
    (exists u, b = Unsafe.Between Bound.MinusInfinity u)
    \/ (exists l1 u1 u, a1 = Unsafe.Between l1 u1 /\ b = Unsafe.Between l1 u).
Proof.
  intros a1 a2 b o2 H.
  destruct a1 as [lower1 upper1 | l1 u1]; destruct a2 as [lower2 upper2 | l2 u2];
    unfold PulseCItv.abduce_le in H.
  - match type of H with
    | (if ?c then _ else _) = _ => destruct c
    end; [ discriminate | ].
    cbn [between] in H. inversion H; subst. right.
    exists lower1, upper1, (Bound.min upper1 upper2). split; reflexivity.
  - destruct (Bound.lt lower1 (Bound.Int l2)); discriminate.
  - destruct (Bound.ge (Bound.Int u1) upper2); [ | discriminate ].
    cbn [between] in H. inversion H; subst. left.
    exists (Bound.min (Bound.Int l1) upper2). reflexivity.
  - discriminate.
Qed.

(** ** L14: the Unsatisfiable case of abduce_lt

    Lean: abduceLt_unsat_sound (Comparison.lean:232), there for two Between operands over
    Z.  Here over PulseCItv.abduce_lt, all four operand shapes, IntLit's own order.

    Only the LEFT operand and the left-hand value need to be in the fragment: the right
    operand is handed to abduce_le untouched, and [ilt x y -> ile (x+1) y] is add_one_le,
    which holds in every area. *)
Theorem abduce_lt_unsat_sound :
  forall (a1 a2 : PulseCItv.t),
    nnu_itv a1 ->
    PulseCItv.abduce_lt a1 a2 = Unsatisfiable ->
    forall x y : IntLit.t, nnu x -> gamma a1 x -> gamma a2 y -> ~ ilt x y.
Proof.
  intros a1 a2 N1 Hunsat x y Nx G1 G2 Hxy.
  unfold PulseCItv.abduce_lt in Hunsat.
  destruct (PulseCItv.abduce_le (PulseCItv.add_int a1 IntLit.one) a2)
    as [| p1 p2] eqn:E.
  - apply (abduce_le_unsat_sound _ _ E (IntLit.add x IntLit.one) y).
    + exact (gamma_add_one _ _ N1 Nx G1).
    + exact G2.
    + rewrite (add_comm x IntLit.one). exact (add_one_le _ _ Hxy).
  - destruct p1 as [b |]; cbn in Hunsat; discriminate.
Qed.

(** ** L15: the Satisfiable case of abduce_lt

    Lean: abduceLt_sat_sound (Comparison.lean:245).

    The left refinement is shifted back by one, and over the tagged order that is where a
    port can go wrong: the shift down is not monotone.  It goes through here because of
    abduce_le_lhs_shape - the only bound that is shifted back on the LOWER side is one the
    caller supplied and this function had just shifted up, so add_one_then_minus_one
    returns it exactly.  The upper side needs nothing, by sat_upper_shift_down. *)
Theorem abduce_lt_sat_sound :
  forall (a1 a2 : PulseCItv.t) (o1 o2 : option PulseCItv.t),
    nnu_itv a1 ->
    PulseCItv.abduce_lt a1 a2 = Satisfiable o1 o2 ->
    forall x y : IntLit.t, nnu x -> gamma a1 x -> gamma a2 y -> ilt x y ->
      refines o1 x /\ refines o2 y.
Proof.
  intros a1 a2 o1 o2 N1 Hsat x y Nx G1 G2 Hxy.
  assert (Gs : gamma (PulseCItv.add_int a1 IntLit.one) (IntLit.add x IntLit.one))
    by exact (gamma_add_one _ _ N1 Nx G1).
  assert (Hle : ile (IntLit.add x IntLit.one) y)
    by (rewrite (add_comm x IntLit.one); exact (add_one_le _ _ Hxy)).
  unfold PulseCItv.abduce_lt in Hsat.
  destruct (PulseCItv.abduce_le (PulseCItv.add_int a1 IntLit.one) a2)
    as [| p1 p2] eqn:E; [ discriminate | ].
  pose proof (abduce_le_sat_sound _ _ _ _ E _ _ Gs G2 Hle) as [R1 R2].
  destruct p1 as [b |]; cbn in Hsat; inversion Hsat; subst; clear Hsat;
    [ | split; [ exact I | exact R2 ] ].
  split; [ | exact R2 ].
  (* refines (Some (add_int b minus_one)) x, from gamma b (x+1) *)
  cbn [refines] in R1 |- *.
  destruct (abduce_le_lhs_shape _ _ _ _ E) as [[u ->] | [l1' [u1' [u [Ha1 ->]]]]].
  - (* the left refinement is bounded below by MinusInfinity: nothing to shift back *)
    destruct R1 as [_ Ru]. cbn [PulseCItv.add_int between gamma Bound.add_int].
    split; [ exact I | exact (sat_upper_shift_down u x Nx Ru) ].
  - (* and otherwise by the caller's own lower bound, shifted up by this very function *)
    destruct R1 as [Rl Ru].
    destruct a1 as [l1 u1 | p q]; cbn [PulseCItv.add_int] in Ha1;
      [ cbn [between] in Ha1 | cbn [outside] in Ha1; discriminate ].
    inversion Ha1; subst; clear Ha1. destruct N1 as [Nl1 Nu1].
    cbn [PulseCItv.add_int between gamma]. rewrite bound_add_int_round_trip. split.
    + apply (sat_lower_add_one_iff l1 x Nl1 Nx). exact Rl.
    + exact (sat_upper_shift_down u x Nx Ru).
Qed.

(** ** L16 and L17: flip_abduced

    Lean: Abduced.flip_unsat and Abduced.flip_sat (Comparison.lean:290, 293).

    flip_abduced is one line and these are two more, so it is worth saying why they are
    here at all.  The dispatch reaches abduce_le and abduce_lt with the operands SWAPPED
    for Ge and Gt, and then puts the two refinements back in the caller's order.  Without
    a statement about the flip, every theorem about the dispatch could be proved of a
    version that dropped it and answered about the wrong operand; the Example at the end
    of this section is the computation that would then differ. *)

Theorem flip_abduced_unsat_sound : forall r,
    PulseCItv.flip_abduced r = Unsatisfiable -> r = Unsatisfiable.
Proof. intros r H. destruct r as [| o1 o2]; [ reflexivity | discriminate ]. Qed.

Theorem flip_abduced_sat_sound : forall r o1 o2,
    PulseCItv.flip_abduced r = Satisfiable o1 o2 -> r = Satisfiable o2 o1.
Proof.
  intros r o1 o2 H. destruct r as [| p1 p2]; [ discriminate | ].
  cbn in H. inversion H; subst. reflexivity.
Qed.

(** ** L18 and L19: the dispatch over the four ordered operators

    Lean: abduceCmp_unsat_sound and abduceCmp_sat_sound (Comparison.lean:314, 328), there
    over a four-constructor model of the operator and with negated fixed to false.

    Here over Binop.t and over BOTH values of [negated], because the real
    abduce_binop_constraints reaches the four helpers from eight rows of its table and the
    eight are not the four repeated: (Gt, true) goes to abduce_le, (Le, true) to the
    flipped abduce_lt, and so on.  A transcription error in that table is exactly the kind
    of defect a proof of the four unnegated rows would not see.

    [negated] means the comparison is being assumed FALSE, so the negated rows state the
    negation of the operator's meaning.  Turning that back into a positive comparison
    needs the order to be total, which it is - the two lemmas below are the only place
    that is used.

    THE nnu HYPOTHESES ARE STATED ON BOTH OPERANDS, AND FOUR OF THE EIGHT ROWS DO NOT NEED
    THEM: the rows that land on abduce_le carry no hypothesis at all, and the rows that
    swap the operands need the fragment on a2 and y rather than on a1 and x.  Saying so
    per row would mean a function from the operator to which side is constrained, which
    buys nothing: sgn_itv_nnu_itv discharges all four at once on the fragment the analyser
    runs in, so the uniform statement is weaker to prove with and no harder to use. *)

Lemma not_ile_ilt : forall x y, ~ ile x y -> ilt y x.
Proof.
  intros x y H. apply ilt_iff.
  destruct (compare_value_spec x y) as [[E K] | [[E [K1 K2]] | [E K]]].
  - exfalso. apply H. apply ile_iff. unfold lexle, lexlt, irank, fst, snd in *. lia.
  - exfalso. apply H. apply ile_iff. unfold lexle, irank, fst, snd in *. lia.
  - unfold lexlt, irank, fst, snd in *. lia.
Qed.

Lemma not_ilt_ile : forall x y, ~ ilt x y -> ile y x.
Proof.
  intros x y H. apply ile_iff.
  destruct (compare_value_spec x y) as [[E K] | [[E [K1 K2]] | [E K]]].
  - exfalso. apply H. apply ilt_iff. exact K.
  - unfold lexle, irank, fst, snd in *. lia.
  - unfold lexle, lexlt, irank, fst, snd in *. lia.
Qed.

(** The meaning of the four ordered operators on concrete literals, and what the dispatch
    is being asked to decide for a given [negated]. *)
Definition ocmp (bop : Binop.t) (x y : IntLit.t) : Prop :=
  match bop with
  | Binop.Le => ile x y
  | Binop.Lt => ilt x y
  | Binop.Ge => ile y x
  | Binop.Gt => ilt y x
  | _ => True
  end.

Definition asserted (negated : bool) (bop : Binop.t) (x y : IntLit.t) : Prop :=
  if negated then ~ ocmp bop x y else ocmp bop x y.

Definition is_ordered (bop : Binop.t) : Prop :=
  bop = Binop.Le \/ bop = Binop.Lt \/ bop = Binop.Ge \/ bop = Binop.Gt.

Theorem abduce_binop_constraints_unsat_sound :
  forall (negated : bool) (bop : Binop.t) (a1 a2 : PulseCItv.t),
    is_ordered bop -> nnu_itv a1 -> nnu_itv a2 ->
    PulseCItv.abduce_binop_constraints negated bop a1 a2 = Unsatisfiable ->
    forall x y : IntLit.t, nnu x -> nnu y -> gamma a1 x -> gamma a2 y ->
      ~ asserted negated bop x y.
Proof.
  intros negated bop a1 a2 Hord N1 N2 H x y Nx Ny G1 G2 A.
  destruct Hord as [-> | [-> | [-> | ->]]]; destruct negated;
    unfold PulseCItv.abduce_binop_constraints in H; cbn [asserted ocmp] in A.
  (* Le, negated: flip_abduced (abduce_lt a2 a1) *)
  - exact (abduce_lt_unsat_sound _ _ N2 (flip_abduced_unsat_sound _ H) y x Ny G2 G1
             (not_ile_ilt _ _ A)).
  (* Le: abduce_le a1 a2 *)
  - exact (abduce_le_unsat_sound _ _ H x y G1 G2 A).
  (* Lt, negated: flip_abduced (abduce_le a2 a1) *)
  - exact (abduce_le_unsat_sound _ _ (flip_abduced_unsat_sound _ H) y x G2 G1
             (not_ilt_ile _ _ A)).
  (* Lt: abduce_lt a1 a2 *)
  - exact (abduce_lt_unsat_sound _ _ N1 H x y Nx G1 G2 A).
  (* Ge, negated: abduce_lt a1 a2 *)
  - exact (abduce_lt_unsat_sound _ _ N1 H x y Nx G1 G2 (not_ile_ilt _ _ A)).
  (* Ge: flip_abduced (abduce_le a2 a1) *)
  - exact (abduce_le_unsat_sound _ _ (flip_abduced_unsat_sound _ H) y x G2 G1 A).
  (* Gt, negated: abduce_le a1 a2 *)
  - exact (abduce_le_unsat_sound _ _ H x y G1 G2 (not_ilt_ile _ _ A)).
  (* Gt: flip_abduced (abduce_lt a2 a1) *)
  - exact (abduce_lt_unsat_sound _ _ N2 (flip_abduced_unsat_sound _ H) y x Ny G2 G1 A).
Qed.

Theorem abduce_binop_constraints_sat_sound :
  forall (negated : bool) (bop : Binop.t) (a1 a2 : PulseCItv.t)
         (o1 o2 : option PulseCItv.t),
    is_ordered bop -> nnu_itv a1 -> nnu_itv a2 ->
    PulseCItv.abduce_binop_constraints negated bop a1 a2 = Satisfiable o1 o2 ->
    forall x y : IntLit.t, nnu x -> nnu y -> gamma a1 x -> gamma a2 y ->
      asserted negated bop x y ->
      refines o1 x /\ refines o2 y.
Proof.
  intros negated bop a1 a2 o1 o2 Hord N1 N2 H x y Nx Ny G1 G2 A.
  destruct Hord as [-> | [-> | [-> | ->]]]; destruct negated;
    unfold PulseCItv.abduce_binop_constraints in H; cbn [asserted ocmp] in A.
  - pose proof (abduce_lt_sat_sound _ _ _ _ N2 (flip_abduced_sat_sound _ _ _ H)
                  y x Ny G2 G1 (not_ile_ilt _ _ A)) as [P Q]. split; assumption.
  - exact (abduce_le_sat_sound _ _ _ _ H x y G1 G2 A).
  - pose proof (abduce_le_sat_sound _ _ _ _ (flip_abduced_sat_sound _ _ _ H)
                  y x G2 G1 (not_ilt_ile _ _ A)) as [P Q]. split; assumption.
  - exact (abduce_lt_sat_sound _ _ _ _ N1 H x y Nx G1 G2 A).
  - exact (abduce_lt_sat_sound _ _ _ _ N1 H x y Nx G1 G2 (not_ile_ilt _ _ A)).
  - pose proof (abduce_le_sat_sound _ _ _ _ (flip_abduced_sat_sound _ _ _ H)
                  y x G2 G1 A) as [P Q]. split; assumption.
  - exact (abduce_le_sat_sound _ _ _ _ H x y G1 G2 (not_ilt_ile _ _ A)).
  - pose proof (abduce_lt_sat_sound _ _ _ _ N2 (flip_abduced_sat_sound _ _ _ H)
                  y x Ny G2 G1 A) as [P Q]. split; assumption.
Qed.

(** ** L20: the witness that pins the flip

    Lean: abduceCmp_ge_puts_the_refinements_in_the_callers_order (Comparison.lean:344).

    [2,2] >= [1,1] must come back with the LEFT refinement {2} and the right one {1}, in
    that order.  The dispatch gets there by calling abduce_le with the operands swapped,
    so without flip_abduced it would answer {1}, {2} - every theorem above would still
    hold of that version, because each one is stated about whichever operand the code
    names, and only a computation can tell the two apart.  Executed on the translated
    code. *)

Definition s2 : IntLit.t := IntLit.t.Build IntLit.Signed 2 IntLit.NotPointer.

Example witness_ge_returns_the_refinements_in_the_callers_order :
  PulseCItv.abduce_binop_constraints false Binop.Ge
    (PulseCItv.equal_to s2) (PulseCItv.equal_to s1)
  = Satisfiable (Some (Unsafe.Between (Bound.Int s2) (Bound.Int s2)))
      (Some (Unsafe.Between (Bound.Int s1) (Bound.Int s1))).
Proof. vm_compute. reflexivity. Qed.

(** ** Why the shift lemmas hypothesise the SHIFTED bound

    The tidier-looking statement - nnu of the bound and of the value, as in the two shift
    UP lemmas - is false, and this is the instance, computed rather than argued.

    u0 is Unsigned 0.  It is in area 2, so it is nnu; n5 is Signed 5, also nnu.  Shifting
    u0 down keeps the Unsigned tag and lands on Unsigned (-1), which is in area 3 and
    therefore ABOVE every literal in areas 1 and 2.  So the two sides of the iff come
    apart, and they come apart in OPPOSITE directions on the two halves of membership:
    as a lower bound the shifted literal admits nothing, as an upper bound it admits
    everything.  That asymmetry is why sat_upper_shift_down needs no side condition and
    there is no sat_lower_shift_down. *)
Example witness_the_shift_down_needs_more_than_nnu :
  nnu u0 /\ nnu n5
  /\ sat_lower (Bound.Int u0) (IntLit.add n5 IntLit.one)
  /\ ~ sat_lower (Bound.add_int (Bound.Int u0) IntLit.minus_one) n5
  /\ sat_upper (Bound.add_int (Bound.Int u0) IntLit.minus_one) n5
  /\ ~ sat_upper (Bound.Int u0) (IntLit.add n5 IntLit.one).
Proof.
  (* apply conj rather than split: split would close the two positive conjuncts by
     conversion on the spot and silently leave four goals for six bullets *)
  repeat apply conj; unfold nnu, sat_lower, sat_upper, ile.
  - vm_compute. discriminate.
  - vm_compute. discriminate.
  - vm_compute. reflexivity.
  - vm_compute. discriminate.
  - vm_compute. reflexivity.
  - vm_compute. discriminate.
Qed.

(** ** The premise, and the half of it that lives outside Rocq

    Every theorem ported above is conditional on its literals being in the nnu fragment.
    That hypothesis costs nothing in the analyser, because every IntLit a Pulse analysis
    can build is Signed and sgn_nnu puts Signed inside nnu.  The claim has two halves and
    they are established in two different places, which is the point of stating them
    together here.

    CLOSURE is machine-checked, below and at sgn_add / sgn_neg / sgn_itv_minus: the
    constructors the translation has produce Signed, lift1 carries the tag through, and
    lift2 joins two of them.

    The BASE CASE is not, and cannot be: this development sees the translated PulseCItv
    and a stand-in IntLit, not infer/src/IR/IntLit.ml, whose real constructors are [of_z]
    and [null].  It is pinned instead by rocq/intlit-signed-invariant.sh, which run.sh
    executes - it asserts that the only places the real module mentions Unsigned are the
    type declaration, the join and a pattern, that the only two record constructions write
    the literal Signed, that IntLit.mli exposes neither the type [signedness] nor the
    fields (so no other module can even NAME the constructor - that half is enforced by
    the OCaml type system, not by a grep), and that the derived t_of_sexp, the one
    remaining way in, has no caller.  The day that check refuses, the hypotheses above
    stop covering the reachable inputs and the theorems say less than they look like
    they say. *)

Theorem signed_literals_are_closed :
  (forall k, sgn (IntLit.of_int k))
  /\ sgn IntLit.zero /\ sgn IntLit.one /\ sgn IntLit.minus_one
  /\ (forall f x, sgn x -> sgn (IntLit.lift1 f x))
  /\ (forall f x y, sgn x -> sgn y -> sgn (IntLit.lift2 f x y)).
Proof.
  repeat apply conj.
  - intros k. reflexivity.
  - reflexivity.
  - reflexivity.
  - reflexivity.
  - intros f x Sx. exact Sx.
  - intros f x y Sx Sy. unfold sgn, IntLit.lift2. cbn.
    unfold sgn in Sx, Sy. rewrite Sx, Sy. reflexivity.
Qed.

(** and so every hypothesis the ported theorems carry is discharged on the fragment the
    arithmetic already runs in *)
Lemma sgn_bound_nnu_bound : forall b, sgn_bound b -> nnu_bound b.
Proof.
  intros b Sb. destruct b as [i | |]; cbn in Sb |- *;
    [ exact (sgn_nnu i Sb) | exact I | exact I ].
Qed.

Lemma sgn_itv_nnu_itv : forall a, sgn_itv a -> nnu_itv a.
Proof.
  intros a Sa. destruct a as [l u | p q]; cbn in Sa |- *; destruct Sa as [S1 S2].
  - split; [ exact (sgn_bound_nnu_bound l S1) | exact (sgn_bound_nnu_bound u S2) ].
  - split; [ exact (sgn_nnu p S1) | exact (sgn_nnu q S2) ].
Qed.

(** sgn_nnu gives one inclusion; this gives the other direction's FAILURE, so "strictly
    weaker" is a checked fact rather than a claim in a comment.  u0 is Unsigned 0: its
    value is non-negative so it is in area 2 and therefore nnu, and its tag is Unsigned so
    it is not sgn.  That literal is the whole reason the shift lemmas above are stated with
    nnu instead of sgn, and the reason sgn_lex and its siblings could be rederived from the
    nnu versions rather than the other way round.

    Each conjunct unfolds only what occurs in it: [unfold sgn] on the first goal, where sgn
    does not appear, is an error rather than a no-op. *)
Example nnu_is_strictly_weaker : nnu u0 /\ ~ sgn u0.
Proof.
  split.
  - unfold nnu, u0. vm_compute. discriminate.
  - unfold sgn, u0. vm_compute. discriminate.
Qed.

(** Integer division and remainder use truncation toward zero, as Zarith does. *)
Example witness_div_negative_positive :
  IntLit.div (IntLit.of_int (-3)) (IntLit.of_int 2) = IntLit.of_int (-1).
Proof. reflexivity. Qed.

Example witness_div_positive_negative :
  IntLit.div (IntLit.of_int 3) (IntLit.of_int (-2)) = IntLit.of_int (-1).
Proof. reflexivity. Qed.

Example witness_div_negative_negative :
  IntLit.div (IntLit.of_int (-3)) (IntLit.of_int (-2)) = IntLit.of_int 1.
Proof. reflexivity. Qed.

Example witness_div_positive_positive :
  IntLit.div (IntLit.of_int 3) (IntLit.of_int 2) = IntLit.of_int 1.
Proof. reflexivity. Qed.

Example witness_rem_negative_positive :
  IntLit.rem (IntLit.of_int (-3)) (IntLit.of_int 2) = IntLit.of_int (-1).
Proof. reflexivity. Qed.

Example witness_rem_positive_negative :
  IntLit.rem (IntLit.of_int 3) (IntLit.of_int (-2)) = IntLit.of_int 1.
Proof. reflexivity. Qed.

Example witness_rem_negative_negative :
  IntLit.rem (IntLit.of_int (-3)) (IntLit.of_int (-2)) = IntLit.of_int (-1).
Proof. reflexivity. Qed.

Example witness_rem_positive_positive :
  IntLit.rem (IntLit.of_int 3) (IntLit.of_int 2) = IntLit.of_int 1.
Proof. reflexivity. Qed.

Example witness_binop_div_negative_numerator :
  PulseCItv.binop Binop.DivI
    (between (Bound.Int (IntLit.of_int (-3))) (Bound.Int (IntLit.of_int (-3))))
    (between (Bound.Int (IntLit.of_int 2)) (Bound.Int (IntLit.of_int 2))) =
  Some (between (Bound.Int (IntLit.of_int (-1))) (Bound.Int (IntLit.of_int (-1)))).
Proof. reflexivity. Qed.

Example witness_binop_div_negative_divisor :
  PulseCItv.binop Binop.DivI
    (between (Bound.Int (IntLit.of_int 3)) (Bound.Int (IntLit.of_int 3)))
    (between (Bound.Int (IntLit.of_int (-2))) (Bound.Int (IntLit.of_int (-2)))) =
  Some (between (Bound.Int (IntLit.of_int (-1))) (Bound.Int (IntLit.of_int (-1)))).
Proof. reflexivity. Qed.

Example witness_binop_rem_negative_numerator :
  PulseCItv.binop Binop.Mod
    (between (Bound.Int (IntLit.of_int (-3))) (Bound.Int (IntLit.of_int (-3))))
    (between (Bound.Int (IntLit.of_int 2)) (Bound.Int (IntLit.of_int 2))) =
  Some (between (Bound.Int (IntLit.of_int (-1))) (Bound.Int IntLit.zero)).
Proof. reflexivity. Qed.

Print Assumptions abduce_lt_unsat_sound.
Print Assumptions abduce_lt_sat_sound.
Print Assumptions abduce_binop_constraints_unsat_sound.
Print Assumptions abduce_binop_constraints_sat_sound.
