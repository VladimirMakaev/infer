(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)

open! IStd
module F = Format
open PulseBasicInterface
module AbductiveDomain = PulseAbductiveDomain
module BaseMemory = PulseBaseMemory
module DecompilerExpr = PulseDecompilerExpr
module ExecutionDomain = PulseExecutionDomain
module PathContext = PulsePathContext

type copy_spec_t =
  | Copied of
      { source_typ: Typ.t option
      ; source_opt: DecompilerExpr.source_expr option
      ; node: Procdesc.Node.t
      ; location: Location.t (* the location to report the issue *)
      ; copied_location: (Procname.t * Location.t) option
            (* [copied_location] has a value when the copied location is different to where to
               report: e.g. this is the case for returning copied values. *)
      ; heap: BaseMemory.t
      ; from: Attribute.CopyOrigin.t
      ; timestamp: Timestamp.t }
  | Modified of
      { source_typ: Typ.t option
      ; source_opt: DecompilerExpr.source_expr option
      ; node: Procdesc.Node.t
      ; location: Location.t
      ; copied_location: (Procname.t * Location.t) option
      ; from: Attribute.CopyOrigin.t
      ; copied_timestamp: Timestamp.t }

type parameter_spec_t =
  | Unmodified of {typ: Typ.t; location: Location.t; heap: BaseMemory.t}
  | Modified

include AbstractDomain.WithBottomTop

val exec :
     t
  -> exec_instr:
       ((ExecutionDomain.t * PathContext.t) * t -> (ExecutionDomain.t * PathContext.t) list * t)
  -> t

val join_to_astate :
  (AbductiveDomain.t * PathContext.t) AbstractDomain.Types.bottom_lifted -> t -> t

val astate_is_bottom : t -> bool

val for_disjunct_exec_instr : t -> t

val pp_with_kind : Pp.print_kind -> F.formatter -> t -> unit

type summary

val make_summary : ProcAttributes.t -> Location.t -> t -> summary

module Summary : sig
  type t = summary [@@deriving yojson_of]

  val bottom : t

  val pp : F.formatter -> t -> unit

  val join : t -> t -> t

  val get_transitive_info_if_not_top : t -> TransitiveInfo.t option

  val has_dropped_disjuncts : t -> bool

  val get_pre_post : t -> AbductiveDomain.Summary.t AbstractDomain.Types.bottom_lifted
end

val add_var :
  Attribute.CopiedInto.t -> source_addr_opt:AbstractValue.t option -> copy_spec_t -> t -> t

val remove_var : Var.t -> t -> t

val mark_intermediates_with_shared_source : (Exp.t * Typ.t) list -> t -> t
(** the copies into the intermediates passed to a call whose source variable is also used by another
    argument of the call are not reported *)

val add_field : Fieldname.t -> source_addr_opt:AbstractValue.t option -> copy_spec_t -> t -> t

val add_parameter : Var.t -> parameter_spec_t -> t -> t

val checked_via_destructor : Var.t -> t -> t

val mark_copies_into_var_as_modified : Var.t -> t -> t
(** the copies into the variable are modified, e.g. it is assigned to *)

val mark_copy_as_modified :
     ?reached_end:bool
  -> is_modified:(BaseMemory.t -> Timestamp.t -> bool)
  -> copied_into:Attribute.CopiedInto.t
  -> source_addr_opt:AbstractValue.t option
  -> t
  -> t

val mark_parameter_as_modified :
  is_modified:(BaseMemory.t -> Timestamp.t -> bool) -> var:Var.t -> t -> t

val get_copied :
     ref_formals:(Pvar.t * Typ.t) list
  -> ptr_formals:(Pvar.t * Typ.t) list
  -> t
  -> ( Attribute.CopiedInto.t
     * Typ.t option
     * DecompilerExpr.source_expr option
     * Procdesc.Node.t
     * Location.t
     * (Procname.t * Location.t) option
     * Attribute.CopyOrigin.t )
     list

val get_const_refable_parameters : t -> (Var.t * Typ.t * Location.t) list

val is_checked_via_destructor : Var.t -> t -> bool

val set_captured_variables : Exp.t -> t -> t

val record_closure_store :
     Location.t
  -> Timestamp.t
  -> rhs_addr:AbstractValue.t
  -> is_escape:(unit -> bool)
  -> AbductiveDomain.t
  -> t
  -> t

val record_closure_load : src:AbstractValue.t -> dst:AbstractValue.t -> AbductiveDomain.t -> t -> t

val record_closure_call :
     Location.t
  -> Timestamp.t
  -> callee:Procname.t option
  -> actuals:AbstractValue.t list
  -> has_new_unknown_effect:(AbstractValue.t -> bool)
  -> AbductiveDomain.t
  -> t
  -> t

val set_locked : t -> t

val is_locked : t -> bool

val set_load : Location.t -> Timestamp.t -> Ident.t -> Var.t -> t -> t

val set_store : Location.t -> Timestamp.t -> Pvar.t -> t -> t

val get_loaded_locations : Var.t -> t -> Location.t list

val set_passed_to : Location.t -> Timestamp.t -> Exp.t -> (Exp.t * Typ.t) list -> t -> t

val is_lifetime_extended : Var.t -> t -> bool

val remember_dropped_disjuncts : (ExecutionDomain.t * PathContext.t) list -> t -> t

val top_keeping_dropped_disjuncts : t -> t
(** [top], but recording dropped disjuncts only if the argument does: having only exited disjuncts
    after a call to a function that does not return is not a reason to treat that call as unknown
    with [--pulse-force-continue] *)

val add_specialized_direct_callee : Procname.t -> Specialization.Pulse.t -> Location.t -> t -> t

val apply_summary :
     callee_pname:Procname.t
  -> call_loc:Location.t
  -> skip_transitive_accesses:bool
  -> t
  -> summary
  -> t

val bind : 'a list * t -> f:('a -> t -> 'b list * t) -> 'b list * t
(** {[
      bind ([astate1; astate2; ...; astateN], non_disj) f =
          (astates1 \@ astate2 \@ ... \@ astatesN, non_disj1 U non_disj2 U ... U non_disjN)
      with
        (astates1, non_disj1) = f astate1 non_disj
        (astates2, non_disj2) = f astate2 non_disj
        ...
        (astatesN, non_disjN) = f astateN non_disj
    ]} *)
