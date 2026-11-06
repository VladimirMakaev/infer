(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *)
open! IStd
module F = Format
module AbstractValue = PulseAbstractValue
module CallEvent = PulseCallEvent
module ConfigName = FbPulseConfigName
module DecompilerExpr = PulseDecompilerExpr
module Invalidation = PulseInvalidation
module TaintItem = PulseTaintItem
module TaintConfig = PulseTaintConfig
module Timestamp = PulseTimestamp
module Trace = PulseTrace
module ValueHistory = PulseValueHistory

type allocator =
  | CMalloc
  | CustomMalloc of Procname.t
  | CRealloc
  | CustomRealloc of Procname.t
  | CppNew
  | CppNewArray
  | JavaResource of JavaClassName.t
  | CSharpResource of CSharpClassName.t
  | ObjCAlloc
  | SwiftAlloc
  | HackBuilderResource of HackClassName.t
  | Awaitable (* used for Hack and Python *)
  | FileDescriptor of Procname.t
[@@deriving compare, equal, yojson_of]

val pp_allocator : F.formatter -> allocator -> unit

val is_hack_resource : allocator -> bool

val is_python_resource : allocator -> bool

(** Describes the source of taint in taint propagation.

    NOTE: [history] is ignored in equality and comparison. *)
type taint_in = {v: AbstractValue.t; history: ValueHistory.t} [@@deriving compare, equal]

module Tainted : sig
  type t =
    { source: TaintItem.t
    ; time_trace: Timestamp.trace
    ; hist: ValueHistory.t
    ; intra_procedural_only: bool }
  [@@deriving compare, equal]
end

module TaintedSet : PrettyPrintable.PPSet with type elt = Tainted.t

module TaintSink : sig
  type t = {sink: TaintItem.value_tuple; time: Timestamp.t; trace: Trace.t}
  [@@deriving compare, equal]
end

module TaintSinkMap : PrettyPrintable.PPMap with type key = TaintConfig.Kind.t

module TaintSanitized : sig
  type t = {sanitizer: TaintItem.t; time_trace: Timestamp.trace; trace: Trace.t}
  [@@deriving compare, equal]
end

module TaintSanitizedSet : PrettyPrintable.PPSet with type elt = TaintSanitized.t

type taint_propagation_reason = InternalModel | UnknownCall | UserConfig

val pp_taint_propagation_reason : F.formatter -> taint_propagation_reason -> unit

module CopyOrigin : sig
  type assign_t = Normal | Thrift [@@deriving compare, equal, yojson_of]

  type t = CopyCtor | CopyAssignment of assign_t | CopyToOptional | CopyInGetDefault
  [@@deriving compare, equal, yojson_of]

  val pp : Formatter.t -> t -> unit
end

module CopiedInto : sig
  type t =
    | IntoVar of {copied_var: Var.t}
    | IntoIntermediate of {copied_var: Var.t}
    | IntoField of {field: Fieldname.t}
  [@@deriving compare, equal, yojson_of]

  val is_copied_into_var : t -> bool

  val pp : F.formatter -> t -> unit
end

module ConfigUsage : sig
  type t = ConfigName of ConfigName.t | StringParam of {v: AbstractValue.t; config_type: string}
  [@@deriving equal]
end

(** Where a value that stands for a FAILURE came from. [source] has two cases by design though only
    [NullableReturn] is produced today, so that adding the general failure contract later extends a
    match rather than changing this type and its attribute rank. *)
module FailureOrigin : sig
  type source = NullableReturn | FailureOutcome [@@deriving compare, equal]

  type t = {source: source; callee: Procname.t; location: Location.t} [@@deriving compare, equal]

  val pp : F.formatter -> t -> unit [@@warning "-unused-value-declaration"]
end

module FailureOriginSet : sig
  include PrettyPrintable.PPSet with type elt = FailureOrigin.t

  val union_capped : t -> t -> t
  (** union, truncated deterministically to a small bound: the single definition of what merging two
      sets of origins means, so a join and an attribute add cannot disagree. *)
end

module Builder : sig
  type t = Discardable | NonDiscardable [@@deriving compare, equal]

  val pp : F.formatter -> t -> unit [@@warning "-unused-value-declaration"]
end

module UninitializedTyp : sig
  type t =
    | Value
    | Array of IntLit.t
    | Const of Fieldname.t
    | DictMissingKey of {dict: DecompilerExpr.t; key: Fieldname.t}
  [@@deriving compare, equal, yojson_of]

  val pp : F.formatter -> t -> unit
end

module ConstKeys : sig
  type t

  val is_empty : t -> bool

  val singleton : Fieldname.t -> Timestamp.t * Trace.t -> t

  val fold : (Fieldname.t -> Timestamp.t * Trace.t -> 'a -> 'a) -> t -> 'a -> 'a

  val inter : t -> t -> t
end

type lookup_key = KnownInteger of IntLit.t | SymbolicInteger of AbstractValue.t
[@@deriving compare, equal, yojson_of]

type t =
  | AddressOfCppTemporary of Var.t * ValueHistory.t
  | AddressOfStackVariable of Var.t * Location.t * ValueHistory.t
  | Allocated of allocator * Trace.t
  | AlwaysReachable
  | Closure of Procname.t
  | ConfigUsage of ConfigUsage.t
  | ContentsOverwritten of ValueHistory.t
      (** the cells reachable from the address through field and array accesses have been given
          unknown values, eg by [memcpy] or [read] with a size that does not give the type of the
          object: cells not yet in the heap are not abduced from the precondition, and callers give
          fresh values to their own view of these cells *)
  | CopiedInto of CopiedInto.t  (** records the copied var/field for each source address *)
  | CopiedReturn of
      {source: AbstractValue.t; is_const_ref: bool; from: CopyOrigin.t; copied_location: Location.t}
      (** records the copied value for the return address *)
  | DictContainConstKeys
      (** the dictionary contains only constant keys (note: only string constant is supported for
          now) *)
  | DictReadConstKeys of ConstKeys.t  (** constant string keys that are read from the dictionary *)
  | EndOfCollection
  | GlobalInitializerCalled
      (** set on the address of a global variable once its initializer has been executed *)
  | HackBuilder of Builder.t
  | HackConstinitCalled
  | InReportedRetainCycle
  | Initialized
  | Invalid of Invalidation.t * Trace.t
  | LastLookup of AbstractValue.t
  | MustBeAwaited
  | MustBeInitialized of Timestamp.t * Trace.t
  | MustBeNonNull of Timestamp.t * Trace.t * CallEvent.t * int
      (** the value is passed to a parameter of the callee, at the given 1-based position counting
          the implicit object parameter, that must not be null; unlike [MustBeValid], only null
          breaks it *)
  | MustBeValid of Timestamp.t * Trace.t * Invalidation.must_be_valid_reason option
  | MustNotBeTainted of TaintSink.t TaintSinkMap.t
  | JavaResourceReleased
  | CSharpResourceReleased
  | AwaitedAwaitable
  | PropagateTaintFrom of taint_propagation_reason * taint_in list
  | ReturnedFromUnknown of AbstractValue.t list
  | SourceOriginOfCopy of {source: PulseAbstractValue.t; is_unmovable_source: bool}
      (** records the source value for a given copy to lookup the appropriate heap in non-disj
          domain; [is_unmovable_source] when the source is a reference to [const] or is not owned by
          the procedure, so that moving from the copy cannot be replaced by moving from the source
      *)
  | StaticType of Typ.Name.t
      (** type gotten or inferred from types in SIL instructions (only for Hack frontend)*)
  | StdMoved
  | StdVectorErased of Trace.t
      (** some elements of this internal array of a [std::vector] were erased; the callers
          invalidate all the elements that they know of *)
  | StdVectorReserve
  | Tainted of TaintedSet.t
  | TaintSanitized of TaintSanitizedSet.t
  | Uninitialized of UninitializedTyp.t
  | UnknownEffect of CallEvent.t * ValueHistory.t
      (** generated by calls to unknown functions to remember that a pointer has been passed to an
          unknown function and so everything reachable from it has potentially been affected in
          unknown ways *)
  | UnreachableAt of Location.t
      (** temporary marker to remember where a variable became unreachable; helps with accurately
          reporting leaks *)
  | UsedAsBranchCond of Procname.t * Location.t * Trace.t
  | WrittenTo of Timestamp.t * Trace.t
  | LastLookupValue of {key: lookup_key; first_key: lookup_key option; known_present: bool}
  | StdVectorCapacity of IntLit.t option
  | AbsentFromContainer
      (** the value a container model invented for a key or element it assumed absent, rather than
          one the program produced. Attribute ranks come from Variants.to_rank, i.e. from
          constructor position, so a new attribute is added at the END: inserting anywhere else
          renumbers every existing attribute and rewrites every summary on disk. *)
  | OwnershipTransferred of CallEvent.t * ValueHistory.t
      (** generated by a call to a function with no body that receives the ownership of what a
          pointer reaches, to remember that this procedure no longer has to release it. Unlike
          [UnknownEffect] it makes no claim about the VALUES reachable from the pointer: a callee
          can adopt a pointer it only ever reads, so nothing is havoc'd and no write is recorded.
          Deliberately LAST, for the rank reason above. *)
  | CppLifetimeBegun
      (** a new C++ object's lifetime began in the storage at this address, e.g. by placement new,
          so a [CppDelete] that the previous occupant left there is stale. Clearing an invalidation
          is a REMOVAL, which a summary cannot express, so the removal is recorded as this positive
          attribute and the caller redoes it on its own state when it applies the summary -- where
          the fields of the destroyed object exist and the callee's state has none of them.
          [invalidate] drops it again, so whichever of the two happened last wins. ALSO LAST, for
          the rank reason above. *)
  | FileDescriptorOwnershipTransferred of CallEvent.t * ValueHistory.t
      (** an unknown constructor adopted this integral value's descriptor. Reapplied to descriptor
          allocations in callers, preserving cast memory obligations. Appended to preserve existing
          attribute ranks. *)
  | FailureOutcome of FailureOriginSet.t
      (** [v] is a value some call produced to mean failure; a set because two paths reaching [v]
          can come from different calls. ALSO LAST, for the rank reason above. *)
[@@deriving compare, equal, yojson_of]

val pp : F.formatter -> t -> unit

val filter_unreachable :
  AbstractValue.Set.t AbstractValue.Map.t -> (AbstractValue.t -> bool) -> t -> t option
(** update an attribute to get rid of abstract values that do not satisfy the given predicate; the
    result is [None] if the attribute becomes meaningless as a result *)

module Attributes : sig
  include PrettyPrintable.PPUniqRankSet with type elt = t

  val get_address_of_stack_variable : t -> (Var.t * Location.t * ValueHistory.t) option

  val get_closure_proc_name : t -> Procname.t option

  val get_config_usage : t -> ConfigUsage.t option

  val get_used_as_branch_cond : t -> (Procname.t * Location.t * Trace.t) option

  val get_copied_into : t -> CopiedInto.t option

  val get_copied_return : t -> (AbstractValue.t * bool * CopyOrigin.t * Location.t) option

  val remove_copied_return : t -> t

  val get_source_origin_of_copy : t -> (PulseAbstractValue.t * bool) option

  val get_allocation : t -> (allocator * Trace.t) option

  val remove_allocation : t -> t

  val get_unknown_effect : t -> (CallEvent.t * ValueHistory.t) option

  val get_ownership_transferred : t -> (CallEvent.t * ValueHistory.t) option

  val get_file_descriptor_ownership_transferred : t -> (CallEvent.t * ValueHistory.t) option

  val get_contents_overwritten : t -> ValueHistory.t option

  val remove_dict_contain_const_keys : t -> t

  val is_dict_contain_const_keys : t -> bool

  val get_dict_read_const_keys : t -> ConstKeys.t option

  val get_static_type : t -> Typ.Name.t option

  val is_java_resource_released : t -> bool

  val is_cpp_lifetime_begun : t -> bool

  val get_hack_builder : t -> Builder.t option

  val remove_hack_builder : t -> t

  val set_hack_builder_discardable : t -> t

  val is_global_initializer_called : t -> bool

  val is_hack_constinit_called : t -> bool

  val is_csharp_resource_released : t -> bool

  val is_end_of_collection : t -> bool

  val get_invalid : t -> (Invalidation.t * Trace.t) option

  val get_tainted : t -> TaintedSet.t

  val remove_tainted : t -> t

  val remove_must_not_be_tainted : ?kinds:TaintConfig.Kind.Set.t -> t -> t

  val get_propagate_taint_from : t -> (taint_propagation_reason * taint_in list) option

  val remove_propagate_taint_from : t -> t

  val get_returned_from_unknown : t -> AbstractValue.t list option

  val get_taint_sanitized : t -> TaintSanitizedSet.t

  val remove_taint_sanitized : t -> t

  val is_must_be_awaited : t -> bool

  val get_must_be_valid :
    t -> (Timestamp.t * Trace.t * Invalidation.must_be_valid_reason option) option

  val remove_must_be_valid : t -> t

  val get_must_be_non_null : t -> (Timestamp.t * Trace.t * CallEvent.t * int) option

  val get_must_not_be_tainted : t -> TaintSink.t TaintSinkMap.t

  val get_written_to : t -> (Timestamp.t * Trace.t) option

  val is_always_reachable : t -> bool

  val is_in_reported_retain_cycle : t -> bool

  val is_modified : t -> bool

  val is_std_moved : t -> bool

  val is_std_vector_reserved : t -> bool

  val get_std_vector_capacity : t -> IntLit.t option option

  val get_std_vector_erased : t -> Trace.t option

  val get_last_lookup : t -> AbstractValue.t option

  val get_last_lookup_value : t -> (lookup_key * lookup_key option * bool) option

  val get_uninitialized : t -> UninitializedTyp.t option

  val remove_uninitialized : t -> t

  val remove_initialized : t -> t

  val remove_must_be_initialized : t -> t

  val get_must_be_initialized : t -> (Timestamp.t * Trace.t) option

  val get_unreachable_at : t -> Location.t option

  val add_call_and_subst :
       (AbstractValue.t -> AbstractValue.t)
    -> Timestamp.t
    -> Procname.t
    -> Location.t
    -> ValueHistory.t
    -> t
    -> t

  val get_allocated_not_freed : t -> (allocator * Trace.t) option

  val make_suitable_for_pre_summary : t -> t

  val make_suitable_for_post_summary : t -> t

  val remove_all_taint_related : t -> t
end
