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

(** On Signed literals the tagged order and the order on the value agree. *)
Lemma sgn_lex : forall x y, sgn x -> sgn y ->
    (x.(IntLit.t.i) <= y.(IntLit.t.i) <-> lexle (irank x) (irank y)).
Proof.
  intros x y Sx Sy. unfold lexle, irank, fst, snd.
  destruct (Z_lt_le_dec x.(IntLit.t.i) 0) as [Hx | Hx];
    destruct (Z_lt_le_dec y.(IntLit.t.i) 0) as [Hy | Hy].
  - rewrite (area_neg_signed x Hx Sx), (area_neg_signed y Hy Sy). lia.
  - rewrite (area_neg_signed x Hx Sx), (area_nonneg y Hy). lia.
  - rewrite (area_nonneg x Hx), (area_neg_signed y Hy Sy). lia.
  - rewrite (area_nonneg x Hx), (area_nonneg y Hy). lia.
Qed.

Lemma sgn_lexlt : forall x y, sgn x -> sgn y ->
    (x.(IntLit.t.i) < y.(IntLit.t.i) <-> lexlt (irank x) (irank y)).
Proof.
  intros x y Sx Sy. unfold lexlt, irank, fst, snd.
  destruct (Z_lt_le_dec x.(IntLit.t.i) 0) as [Hx | Hx];
    destruct (Z_lt_le_dec y.(IntLit.t.i) 0) as [Hy | Hy].
  - rewrite (area_neg_signed x Hx Sx), (area_neg_signed y Hy Sy). lia.
  - rewrite (area_neg_signed x Hx Sx), (area_nonneg y Hy). lia.
  - rewrite (area_nonneg x Hx), (area_neg_signed y Hy Sy). lia.
  - rewrite (area_nonneg x Hx), (area_nonneg y Hy). lia.
Qed.

Lemma sgn_ile : forall x y, sgn x -> sgn y ->
    (ile x y <-> x.(IntLit.t.i) <= y.(IntLit.t.i)).
Proof.
  intros x y Sx Sy. rewrite ile_iff. split; intro H.
  - apply (sgn_lex x y Sx Sy). exact H.
  - apply (sgn_lex x y Sx Sy). exact H.
Qed.

Lemma sgn_ilt : forall x y, sgn x -> sgn y ->
    (ilt x y <-> x.(IntLit.t.i) < y.(IntLit.t.i)).
Proof.
  intros x y Sx Sy. rewrite ilt_iff. split; intro H.
  - apply (sgn_lexlt x y Sx Sy). exact H.
  - apply (sgn_lexlt x y Sx Sy). exact H.
Qed.

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

(** Everything else declines.  Eighteen operators, which is most of binop's surface. *)
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
