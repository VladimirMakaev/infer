(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

(** A bounded soundness oracle for the concrete interval domain.

    Pulse uses this domain to decide that a comparison is impossible, which prunes a path, and to
    tighten an operand when a comparison holds. Both decisions are only as good as the arithmetic
    underneath, and an interval that is tightened too far or a contradiction announced too eagerly
    removes real program states and so removes real reports.

    This oracle states what each exported function means as a set of integers, independently of how
    the module computes it, and then compares the two over every interval that can be built from a
    small window of endpoints. The definitions below never call the module: [mem] is a reading of
    the two constructors, and every property is phrased against it.

    Every answer the module gives is judged, including the operators it currently declines: a
    decline is always sound, but an answer is only sound if it holds the true result. An operator
    whose answers this oracle cannot read as a set of integers is instead required to decline.

    A clean run is evidence over the stated window, not a proof. The companion Lean development
    carries the unbounded statement for the ordered-comparison fragment. *)

module C = Citv_open.PulseCItv
module B = C.Bound

let z = IntLit.of_int

(** {1 Reporting}

    Two numbers are kept apart on purpose. The violation counts are complete; the witnesses are the
    first few violations kept for the reader and are capped, so the cap can never be mistaken for
    the number of things that went wrong. *)

type kind =
  | Wrong  (** the module answered, and the answer does not hold a value it must hold *)
  | Raised  (** the module raised on a value its own constructors built *)

let checks : (string, int) Hashtbl.t = Hashtbl.create 32

let violations : (string, int) Hashtbl.t = Hashtbl.create 32

let witness_cap = 40

let witnesses : (string * string) list ref = ref []

let n_witnesses = ref 0

let n_wrong = ref 0

let n_raised = ref 0

let bump tbl name =
  Hashtbl.replace tbl name (1 + Option.value (Hashtbl.find_opt tbl name) ~default:0)


let count name = bump checks name

let record kind name detail =
  bump violations name ;
  (match kind with Wrong -> incr n_wrong | Raised -> incr n_raised) ;
  if !n_witnesses < witness_cap then (
    incr n_witnesses ;
    witnesses := (name, detail) :: !witnesses )


let fail name detail = record Wrong name detail

let raised name what e =
  record Raised name (Printf.sprintf "%s raised %s" what (Printexc.to_string e))


(* The module is total on every value its own constructors can build: no exported function may
   raise on one, and in particular none may build an interval whose bounds are crossed, which its
   [Unsafe] constructors assert against. An escaping exception is therefore a violation in its own
   right, not a reason to stop judging. Without this, a defect that trips an internal assertion
   before producing a wrong answer would look like a pass.

   The description of what was being done is a thunk, and is built inside the handler, because
   building it prints the operands and printing is itself a call into the module. *)
let guard name what f = try f () with e -> raised name (what ()) e

(* Printing an operand must never be able to escape a guard, and a printer that raises must still
   be reported, so the one safe printer does both. *)
let str_one v =
  try Format.asprintf "%a" C.pp v
  with e ->
    raised "pp" "pp" e ;
    "<pp raised>"


let str_v = function None -> "<absent>" | Some v -> str_one v

(** {1 The independent reading of an interval} *)

(* [Between (l, u)] is the set of integers between its bounds and [Outside (l, u)] is the
   complement of a closed interval; [None] stands for an operand the caller knows nothing
   about, which is every integer. *)
let mem_z (v : C.t) (x : Z.t) : bool =
  match v with
  | C.Between (lo, hi) ->
      let above =
        match lo with
        | B.MinusInfinity ->
            true
        | B.PlusInfinity ->
            false
        | B.Int l ->
            IntLit.leq l x
      in
      let below =
        match hi with
        | B.PlusInfinity ->
            true
        | B.MinusInfinity ->
            false
        | B.Int h ->
            IntLit.leq x h
      in
      above && below
  | C.Outside (l, u) ->
      IntLit.lt x l || IntLit.gt x u


(* Membership is total and is defined for every integer, including one far outside the window,
   so a result the module gets wrong only at a large value is still judged. *)
let mem (v : C.t) (x : int) : bool = mem_z v (z x)

let mem_opt (v : C.t option) (x : int) : bool = match v with None -> true | Some v -> mem v x

(** {1 The value space} *)

(* Every interval the safe constructors can build from endpoints in [-w, w], plus the four
   half-bounded and unbounded shapes, plus the absent operand. The constructors are themselves
   part of the module, so a constructor that raises is a violation and the value is dropped,
   rather than taking the whole run down before anything has been judged. *)
let values ~w : C.t option list =
  let ints = List.init ((2 * w) + 1) (fun i -> i - w) in
  let build what f =
    try Some (f ())
    with e ->
      raised "constructors" what e ;
      None
  in
  let pairs what f =
    List.concat_map
      (fun a ->
        List.filter_map
          (fun b ->
            if a <= b then build (Printf.sprintf "%s %d %d" what a b) (fun () -> f a b) else None )
          ints )
      ints
  in
  let bounded = pairs "between" (fun a b -> C.between (B.Int (z a)) (B.Int (z b))) in
  let outside = pairs "outside" (fun a b -> C.outside (z a) (z b)) in
  let lower_open =
    List.filter_map
      (fun b ->
        build (Printf.sprintf "between -inf %d" b) (fun () ->
            C.between B.MinusInfinity (B.Int (z b)) ) )
      ints
  in
  let upper_open =
    List.filter_map
      (fun a ->
        build (Printf.sprintf "between %d +inf" a) (fun () ->
            C.between (B.Int (z a)) B.PlusInfinity ) )
      ints
  in
  let unbounded =
    Option.to_list (build "between -inf +inf" (fun () -> C.between B.MinusInfinity B.PlusInfinity))
  in
  None :: List.map (fun v -> Some v) (bounded @ outside @ lower_open @ upper_open @ unbounded)


let concrete ~t = List.init ((2 * t) + 1) (fun i -> i - t)

let elements_of ~t v = List.filter (mem v) (concrete ~t)

let elements_of_opt ~t v = List.filter (mem_opt v) (concrete ~t)

(** {1 What the operators mean}

    The oracle's own reading of every operator the module matches on, so that an answer is judged
    wherever the module gives one. *)

type meaning =
  | Judged of (int -> int -> Z.t option)
      (** the concrete result, with [None] at the pairs where the operation has no value at all
          (division by zero, a negative shift count); those pairs are not judged. The result is
          arbitrary precision, so there is no window of operands for which the oracle has to stop
          judging: [1 lsl 17] is a number like any other, and so is a product that would not fit in
          a machine word *)
  | Must_decline
      (** the operators this oracle can give no reading as a set of integers: pointer arithmetic,
          whose operands are addresses rather than numbers, and float division, whose result is not
          an integer. For those the property asserted is that the module answers [None]. That is a
          tripwire, not a soundness statement: if the module ever starts answering them this check
          fails, which is the prompt to extend the oracle rather than to trust the answer. *)

let bop_name (b : Binop.t) =
  match b with
  | Binop.PlusA _ ->
      "PlusA"
  | PlusPI ->
      "PlusPI"
  | MinusA _ ->
      "MinusA"
  | MinusPI ->
      "MinusPI"
  | MinusPP ->
      "MinusPP"
  | Mult _ ->
      "Mult"
  | DivI ->
      "DivI"
  | DivF ->
      "DivF"
  | Mod ->
      "Mod"
  | Shiftlt ->
      "Shiftlt"
  | Shiftrt ->
      "Shiftrt"
  | Lt ->
      "Lt"
  | Gt ->
      "Gt"
  | Le ->
      "Le"
  | Ge ->
      "Ge"
  | Eq ->
      "Eq"
  | Ne ->
      "Ne"
  | BAnd ->
      "BAnd"
  | BXor ->
      "BXor"
  | BOr ->
      "BOr"
  | LAnd ->
      "LAnd"
  | LOr ->
      "LOr"


let uop_name (u : Unop.t) = match u with Unop.Neg -> "Neg" | BNot -> "BNot" | LNot -> "LNot"

(* [bop_name] deliberately collapses the two payload shapes of the three operators that carry
   one, because a report about [PlusA] means the same thing either way. An attestation must not
   collapse them, so it says which shape it is talking about. *)
let payload_shape (b : Binop.t) =
  match b with
  | Binop.PlusA None | MinusA None | Mult None ->
      " None"
  | PlusA (Some ()) | MinusA (Some ()) | Mult (Some ()) ->
      " (Some _)"
  | PlusPI
  | MinusPI
  | MinusPP
  | DivI
  | DivF
  | Mod
  | Shiftlt
  | Shiftrt
  | Lt
  | Gt
  | Le
  | Ge
  | Eq
  | Ne
  | BAnd
  | BXor
  | BOr
  | LAnd
  | LOr ->
      ""


let eval_cmp (bop : Binop.t) x y =
  match bop with
  | Binop.Eq ->
      x = y
  | Ne ->
      x <> y
  | Le ->
      x <= y
  | Lt ->
      x < y
  | Ge ->
      x >= y
  | Gt ->
      x > y
  | _ ->
      assert false


let is_cmp (bop : Binop.t) = match bop with Binop.Eq | Ne | Le | Lt | Ge | Gt -> true | _ -> false

(* The arithmetic is done in arbitrary precision, as the module's own is: [Z.div] truncates
   toward zero and [Z.rem] keeps the sign of the dividend, which is what C says, and they are
   the very operations [IntLit.div] is built from. Doing it in a machine word would mean having
   to stop judging somewhere, and a bound on what the oracle will judge is a window in which a
   wrong answer is accepted.

   The bitwise and shift operators are read as two's complement on mathematical integers, which
   is what every target this analyser runs on does. Where C leaves the result undefined - a
   division by zero, a negative shift count - the oracle judges nothing rather than inventing a
   value; a shift count larger than a machine word is not one of those cases, because the value
   [1 lsl 17] denotes is perfectly well defined and the module is free to be wrong about it. *)
let binop_meaning (bop : Binop.t) =
  let z = Z.of_int in
  let truth b = Some (if b then Z.one else Z.zero) in
  match bop with
  | Binop.PlusA _ ->
      Judged (fun x y -> Some (Z.add (z x) (z y)))
  | MinusA _ ->
      Judged (fun x y -> Some (Z.sub (z x) (z y)))
  | Mult _ ->
      Judged (fun x y -> Some (Z.mul (z x) (z y)))
  | DivI ->
      Judged (fun x y -> if y = 0 then None else Some (Z.div (z x) (z y)))
  | Mod ->
      Judged (fun x y -> if y = 0 then None else Some (Z.rem (z x) (z y)))
  | Shiftlt ->
      Judged (fun x y -> if y < 0 then None else Some (Z.shift_left (z x) y))
  | Shiftrt ->
      Judged (fun x y -> if y < 0 then None else Some (Z.shift_right (z x) y))
  | BAnd ->
      Judged (fun x y -> Some (Z.logand (z x) (z y)))
  | BXor ->
      Judged (fun x y -> Some (Z.logxor (z x) (z y)))
  | BOr ->
      Judged (fun x y -> Some (Z.logor (z x) (z y)))
  | LAnd ->
      Judged (fun x y -> truth (x <> 0 && y <> 0))
  | LOr ->
      Judged (fun x y -> truth (x <> 0 || y <> 0))
  | Lt | Gt | Le | Ge | Eq | Ne ->
      Judged (fun x y -> truth (eval_cmp bop x y))
  | PlusPI | MinusPI | MinusPP | DivF ->
      Must_decline


(* Every unary operator has a reading, so there is no [Must_decline] here. Arbitrary precision
   again, for the same reason as [binop_meaning]. *)
let unop_eval (op : Unop.t) x =
  match op with
  | Unop.Neg ->
      Z.neg (Z.of_int x)
  | BNot ->
      Z.lognot (Z.of_int x)
  | LNot ->
      if x = 0 then Z.one else Z.zero


(* [abduce_binop_is_true] takes any operator, not only a comparison: its dispatch ends in a
   catch-all that declines. "x bop y is true" means the comparison holds for a comparison, and
   the value is non-zero for an operator that produces one, as in C. [None] where there is no
   reading, either because the operation has no value at that pair or because the operator is
   outside the fragment; those pairs are not judged, and an operator with no reading anywhere
   must not be called unsatisfiable. *)
let holds_as_condition (bop : Binop.t) x y =
  if is_cmp bop then Some (eval_cmp bop x y)
  else
    match binop_meaning bop with
    | Must_decline ->
        None
    | Judged eval ->
        Option.map (fun r -> not (Z.equal r Z.zero)) (eval x y)


let has_condition_reading (bop : Binop.t) =
  is_cmp bop || match binop_meaning bop with Must_decline -> false | Judged _ -> true


(** {1 The properties, one per exported value} *)

(* [unknown] constrains nothing, [equal_to i] is exactly [i], [nonnegative] is [0, +inf). Each one
   is a call into the module, so each one is guarded on its own: a constant that raises must be
   reported and must not stop the other two from being judged. *)
let check_unknown ~t =
  count "unknown" ;
  guard "unknown" (fun () -> "unknown")
  @@ fun () ->
  List.iter
    (fun x -> if not (mem C.unknown x) then fail "unknown" (Printf.sprintf "%d is not in unknown" x))
    (concrete ~t)


let check_equal_to ~t i =
  count "equal_to" ;
  guard "equal_to" (fun () -> Printf.sprintf "equal_to %d" i)
  @@ fun () ->
  let v = C.equal_to (z i) in
  List.iter
    (fun x ->
      if Bool.equal (mem v x) (x = i) then ()
      else fail "equal_to" (Printf.sprintf "equal_to %d disagrees at %d" i x) )
    (concrete ~t)


let check_nonnegative ~t =
  count "nonnegative" ;
  guard "nonnegative" (fun () -> "nonnegative")
  @@ fun () ->
  List.iter
    (fun x ->
      if Bool.equal (mem C.nonnegative x) (x >= 0) then ()
      else fail "nonnegative" (Printf.sprintf "nonnegative disagrees at %d" x) )
    (concrete ~t)


let check_constants ~t =
  check_unknown ~t ;
  List.iter (check_equal_to ~t) (concrete ~t) ;
  check_nonnegative ~t


let check_predicates ~t v =
  guard "predicates" (fun () -> Printf.sprintf "a predicate on %s" (str_one v))
  @@ fun () ->
  (* Each predicate claims something about the set; claiming it must make it true. *)
  let els = elements_of ~t v in
  count "is_equal_to_zero" ;
  if C.is_equal_to_zero v && not (List.equal Int.equal els [0]) then
    fail "is_equal_to_zero"
      (Printf.sprintf "%s says =0 but holds %d values" (str_one v) (List.length els)) ;
  count "is_not_equal_to_zero" ;
  if C.is_not_equal_to_zero v && mem v 0 then
    fail "is_not_equal_to_zero" (Printf.sprintf "%s says <>0 but contains 0" (str_one v)) ;
  count "is_non_pointer" ;
  ( if C.is_non_pointer v then
      (* a non-pointer interval has two finite bounds, so widening the window cannot add members *)
      let wide = List.filter (mem v) (concrete ~t:(2 * t)) in
      if List.length wide <> List.length els then
        fail "is_non_pointer"
          (Printf.sprintf "%s claims finite bounds but grows with the window" (str_one v)) ) ;
  count "to_singleton" ;
  ( match C.to_singleton v with
  | Some i ->
      let i = IntLit.to_int i in
      if not (List.equal Int.equal els [i]) then
        fail "to_singleton"
          (Printf.sprintf "%s says singleton %d but holds %d values" (str_one v) i (List.length els))
  | None ->
      () ) ;
  (* [pp] is a printer, not a denotation: the only property asserted is that it is total. The
     safe printer reports a raise on its own, so calling it here is the whole check. *)
  count "pp" ;
  ignore (str_one v : string) ;
  (* [requires_integer_reasoning] is a representability heuristic with no set-level meaning, so
     only totality and determinism are asserted; see README.md. *)
  count "requires_integer_reasoning" ;
  let a = C.requires_integer_reasoning v in
  let b = C.requires_integer_reasoning v in
  if not (Bool.equal a b) then fail "requires_integer_reasoning" (str_one v)


let check_intersection ~t a b =
  count "intersection" ;
  guard "intersection" (fun () ->
      Printf.sprintf "intersection of %s and %s" (str_one a) (str_one b) )
  @@ fun () ->
  let both = List.filter (fun x -> mem a x && mem b x) (concrete ~t) in
  match C.intersection a b with
  | Some c ->
      (* The domain has no hole: intersecting [-2,1] with <>0 cannot be represented exactly, so
         the contract is containment, not equality. Losing a shared value would be unsound;
         keeping extra ones is only imprecise. *)
      List.iter
        (fun x ->
          if not (mem c x) then
            fail "intersection"
              (Printf.sprintf "%s and %s share %d, but their intersection %s drops it" (str_one a)
                 (str_one b) x (str_one c) ) )
        both
  | None ->
      if not (List.is_empty both) then
        fail "intersection"
          (Printf.sprintf "%s and %s called disjoint but share %d" (str_one a) (str_one b)
             (List.hd both) )


let check_binop ~t bop a b =
  count "binop" ;
  guard "binop" (fun () -> Printf.sprintf "%s %s %s" (str_one a) (bop_name bop) (str_one b))
  @@ fun () ->
  match (binop_meaning bop, C.binop bop a b) with
  | _, None ->
      () (* declining to answer is always sound *)
  | Must_decline, Some c ->
      fail "binop/decline"
        (Printf.sprintf "%s %s %s answers %s, but this oracle has no reading of %s to judge it by"
           (str_one a) (bop_name bop) (str_one b) (str_one c) (bop_name bop) )
  | Judged eval, Some c ->
      let xs = elements_of ~t a and ys = elements_of ~t b in
      List.iter
        (fun x ->
          List.iter
            (fun y ->
              match eval x y with
              | Some r when not (mem_z c r) ->
                  fail "binop"
                    (Printf.sprintf "%s %s %s gives %s, which misses %d %s %d = %s" (str_one a)
                       (bop_name bop) (str_one b) (str_one c) x (bop_name bop) y (Z.to_string r) )
              | _ ->
                  () )
            ys )
        xs


let check_unop ~t op a =
  count "unop" ;
  guard "unop" (fun () -> Printf.sprintf "%s %s" (uop_name op) (str_one a))
  @@ fun () ->
  match C.unop op a with
  | None ->
      ()
  | Some c ->
      List.iter
        (fun x ->
          let r = unop_eval op x in
          if not (mem_z c r) then
            fail "unop"
              (Printf.sprintf "%s %s gives %s, which misses %s %d = %s" (uop_name op) (str_one a)
                 (str_one c) (uop_name op) x (Z.to_string r) ) )
        (elements_of ~t a)


let check_abduce ~t ~negated bop a b =
  count "abduce_binop_is_true" ;
  guard "abduce" (fun () ->
      Printf.sprintf "%s %s %s negated=%b" (str_v a) (bop_name bop) (str_v b) negated )
  @@ fun () ->
  let ga = elements_of_opt ~t a and gb = elements_of_opt ~t b in
  let feasible =
    List.concat_map
      (fun x ->
        List.filter_map
          (fun y ->
            match holds_as_condition bop x y with
            | Some h when Bool.equal h (not negated) ->
                Some (x, y)
            | _ ->
                None )
          gb )
      ga
  in
  match C.abduce_binop_is_true ~negated bop a b with
  | C.Unsatisfiable ->
      if not (has_condition_reading bop) then
        fail "abduce/decline"
          (Printf.sprintf
             "%s %s %s negated=%b called impossible, but this oracle has no reading of %s as a \
              condition to judge it by"
             (str_v a) (bop_name bop) (str_v b) negated (bop_name bop) )
      else if not (List.is_empty feasible) then
        let x, y = List.hd feasible in
        fail "abduce/unsat"
          (Printf.sprintf "%s %s %s negated=%b called impossible, but (%d, %d) satisfies it"
             (str_v a) (bop_name bop) (str_v b) negated x y )
  | C.Satisfiable (a', b') ->
      let keeps side refined live =
        match refined with
        | None ->
            ()
        | Some r ->
            List.iter
              (fun x ->
                if not (mem r x) then
                  fail "abduce/refine"
                    (Printf.sprintf "%s %s %s negated=%b tightened the %s to %s, dropping %d"
                       (str_v a) (bop_name bop) (str_v b) negated side (str_one r) x ) )
              live
      in
      keeps "left" a' (List.sort_uniq Int.compare (List.map fst feasible)) ;
      keeps "right" b' (List.sort_uniq Int.compare (List.map snd feasible))


(** {1 Driver} *)

(* {1 Attesting the domains}

   Four rounds of review found four different domains this oracle did not fully visit, and
   every one of them was invisible: a sampler that skips a value reports no failure for it, and
   a run that reports no failures prints exactly like a run that found none. Counting is not
   enough either - a check that takes its expected size from the thing it is checking accepts
   every prefix of it.

   So every enumerated domain is now attested by SET EQUALITY against an expectation computed
   by a route that shares no code with the sampler, before anything is judged. Missing and
   duplicate both fail: a sampler that produced one value twice produced some other value never,
   and the totals would still look right. *)

let attest_set name ~compare ~pp ~expected ~actual =
  let sorted = List.sort compare in
  let e = sorted expected and a = sorted actual in
  let rec dups = function
    | x :: (y :: _ as tl) ->
        if compare x y = 0 then x :: dups tl else dups tl
    | _ ->
        []
  in
  let missing = List.filter (fun x -> not (List.exists (fun y -> compare x y = 0) a)) e in
  let extra = List.filter (fun x -> not (List.exists (fun y -> compare x y = 0) e)) a in
  let repeated = dups a in
  let say what l =
    if not (List.is_empty l) then
      fail "domain" (Printf.sprintf "%s: %d %s, e.g. %s" name (List.length l) what (pp (List.hd l)))
  in
  say "missing" missing ;
  say "not expected" extra ;
  say "produced more than once" repeated ;
  (* the size of the domain, which is what the driver iterates, not the size of the sampler *)
  List.length e


(* {2 The operand values}

   [values] builds the value space by calling the module's own constructors. The expectation
   below never calls them: it writes down what the space should contain as text, straight from
   the integers, and the actual set is read back out of the constructors of what was built. The
   two agree only if every interval that can be made from the window was made exactly once. *)

let descr_bound (b : B.t) =
  match b with B.MinusInfinity -> "-inf" | B.PlusInfinity -> "+inf" | B.Int i -> Z.to_string i


let descr (v : C.t) =
  match v with
  | C.Between (lo, hi) ->
      Printf.sprintf "B(%s,%s)" (descr_bound lo) (descr_bound hi)
  | C.Outside (l, u) ->
      Printf.sprintf "O(%s,%s)" (Z.to_string l) (Z.to_string u)


let expected_descrs ~w =
  let ints = List.init ((2 * w) + 1) (fun i -> i - w) in
  let ordered_pairs =
    List.concat_map
      (fun a -> List.filter_map (fun b -> if a <= b then Some (a, b) else None) ints)
      ints
  in
  List.map (fun (a, b) -> Printf.sprintf "B(%d,%d)" a b) ordered_pairs
  @ List.map (fun (a, b) -> Printf.sprintf "O(%d,%d)" a b) ordered_pairs
  @ List.map (fun b -> Printf.sprintf "B(-inf,%d)" b) ints
  @ List.map (fun a -> Printf.sprintf "B(%d,+inf)" a) ints
  @ ["B(-inf,+inf)"]


(* {2 The literal set}

   [concrete ~t] is the integers a result is judged at, and the literals [equal_to] and the two
   division helpers are given. Re-deriving it would prove nothing, so it is characterised
   instead: the right length, strictly increasing, and reaching both ends of the window. *)
let attest_literals ~t xs =
  let n = List.length xs in
  let say d = fail "domain" (Printf.sprintf "the literal set: %s" d) in
  if n <> (2 * t) + 1 then say (Printf.sprintf "has %d values, not %d" n ((2 * t) + 1)) ;
  if not (List.equal Int.equal (List.sort_uniq Int.compare xs) xs) then
    say "is not strictly increasing, so it repeats or is out of order" ;
  ( match xs with
  | [] ->
      say "is empty"
  | hd :: _ ->
      if hd <> -t then say (Printf.sprintf "starts at %d, not %d" hd (-t)) ) ;
  match List.rev xs with
  | [] ->
      ()
  | last :: _ ->
      if last <> t then say (Printf.sprintf "ends at %d, not %d" last t)


(* {2 The properties}

   A property whose loop never ran leaves no trace: its name is simply absent from the table,
   and the run still says PASS. Every property this oracle claims to check is named here and
   must have been exercised at least once. *)
let expected_properties =
  [ "unknown"
  ; "equal_to"
  ; "nonnegative"
  ; "is_equal_to_zero"
  ; "is_not_equal_to_zero"
  ; "is_non_pointer"
  ; "to_singleton"
  ; "pp"
  ; "requires_integer_reasoning"
  ; "intersection"
  ; "binop"
  ; "unop"
  ; "abduce_binop_is_true" ]


let attest_properties () =
  List.iter
    (fun name ->
      match Hashtbl.find_opt checks name with
      | Some n when n > 0 ->
          ()
      | _ ->
          fail "coverage" (Printf.sprintf "the property %s was never exercised" name) )
    expected_properties


let () =
  let w = try int_of_string Sys.argv.(1) with _ -> 4 in
  let t = try int_of_string Sys.argv.(2) with _ -> 8 in
  let vs = values ~w in
  let somes = List.filter_map (fun v -> v) vs in
  let negations = [false; true] in
  (* Every domain, attested before anything is judged. [Binop.all] and [Unop.all] come from
     the type declarations; the walks in the same shims are the independent second enumeration.
     The operand values are read back out of their own constructors and matched against an
     expectation written straight from the integers. *)
  let n_binops =
    attest_set "Binop.t" ~compare:Binop.compare
      ~pp:(fun b -> bop_name b ^ payload_shape b)
      ~expected:Binop.all ~actual:Binop.walk
  in
  let n_unops =
    attest_set "Unop.t" ~compare:Unop.compare ~pp:uop_name ~expected:Unop.all ~actual:Unop.walk
  in
  ignore
    ( attest_set
        (Printf.sprintf "the operand values for window %d" w)
        ~compare:String.compare ~pp:Fun.id ~expected:(expected_descrs ~w)
        ~actual:(List.map descr somes)
      : int ) ;
  if List.length vs <> List.length somes + 1 then
    fail "domain" "the absent operand is not offered exactly once alongside the values" ;
  attest_literals ~t (concrete ~t) ;
  if not (List.equal Bool.equal negations [false; true]) then
    fail "domain" "the negated flag is not offered both ways" ;
  check_constants ~t ;
  List.iter (check_predicates ~t) somes ;
  List.iter (fun a -> List.iter (fun b -> check_intersection ~t a b) somes) somes ;
  List.iter (fun a -> List.iter (fun op -> check_unop ~t op a) Unop.all) somes ;
  List.iter
    (fun a -> List.iter (fun b -> List.iter (fun op -> check_binop ~t op a b) Binop.all) somes)
    somes ;
  (* [abduce_binop_is_true] takes any operator, not only a comparison, so it is asked about all
     of them and about both values of [negated]. *)
  List.iter
    (fun a ->
      List.iter
        (fun b ->
          List.iter
            (fun op -> List.iter (fun n -> check_abduce ~t ~negated:n op a b) negations)
            Binop.all )
        vs )
    vs ;
  attest_properties () ;
  let total = Hashtbl.fold (fun _ n acc -> acc + n) checks 0 in
  let sorted tbl =
    Hashtbl.fold (fun k v acc -> (k, v) :: acc) tbl []
    |> List.sort (fun (a, _) (b, _) -> String.compare a b)
  in
  Printf.printf "endpoints in [-%d,%d], integers checked over [-%d,%d]\n" w w t t ;
  Printf.printf "operator shapes visited: %d of Binop.t, %d of Unop.t; interval values: %d\n"
    n_binops n_unops (List.length somes) ;
  Printf.printf "%-28s %s\n" "property" "checks" ;
  List.iter (fun (k, v) -> Printf.printf "%-28s %d\n" k v) (sorted checks) ;
  Printf.printf "%-28s %d\n" "TOTAL" total ;
  let n = !n_wrong + !n_raised in
  Printf.printf "soundness violations: %d\n" n ;
  Printf.printf "  wrong answers:       %d\n" !n_wrong ;
  Printf.printf "  escaping exceptions: %d\n" !n_raised ;
  if n > 0 then (
    Printf.printf "by property:\n" ;
    List.iter (fun (k, v) -> Printf.printf "  %-26s %d\n" k v) (sorted violations) ) ;
  Printf.printf "witnesses retained: %d of %d (cap %d)\n" !n_witnesses n witness_cap ;
  List.iter (fun (k, d) -> Printf.printf "  [%s] %s\n" k d) (List.rev !witnesses) ;
  if n > 0 then exit 1
