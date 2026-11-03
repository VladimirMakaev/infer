/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <utility>

namespace unnecessary_copy_unmovable {

// Declaring a copy constructor suppresses the implicit move constructor, so this type can only
// ever be copied.
struct NoMove {
  const char* n_;
  NoMove(const char* n);
  NoMove(const NoMove&);
  ~NoMove();
};

// The same type with a move constructor, so that the two differ in nothing else.
struct HasMove {
  const char* n_;
  HasMove(const char* n);
  HasMove(const HasMove&);
  HasMove(HasMove&&);
  ~HasMove();
};

struct BaseNoMove {
  explicit BaseNoMove(NoMove p);
};

struct BaseHasMove {
  explicit BaseHasMove(HasMove p);
};

// Forwarding a by-value parameter to a by-value base parameter copies it. The copy can be
// avoided, so it is reported.
struct ForwardNoMove : BaseNoMove {
  explicit ForwardNoMove(NoMove p) : BaseNoMove(p) {}
};

struct ForwardHasMove : BaseHasMove {
  explicit ForwardHasMove(HasMove p) : BaseHasMove(p) {}
};

// Moving out of the parameter is what the report asks for, and for a movable type it works:
// the move constructor runs instead of the copy constructor and nothing is reported.
struct MovedHasMove : BaseHasMove {
  explicit MovedHasMove(HasMove p) : BaseHasMove(std::move(p)) {}
};

struct CastHasMove : BaseHasMove {
  explicit CastHasMove(HasMove p) : BaseHasMove(static_cast<HasMove&&>(p)) {}
};

// For a type with no move constructor it does not work: the argument is an xvalue, but the
// by-value parameter still has to be copy-constructed, because copying is the only thing the
// type can do. The copy is still reported, because one remedy is left -- the callee can take
// the parameter by reference -- and that is the remedy the message now names.
struct MovedNoMove : BaseNoMove {
  explicit MovedNoMove(NoMove p) : BaseNoMove(std::move(p)) {}
};

// The same, written as the cast that [std::move] expands to. Both spellings produce an xvalue
// and both are reported, but they do not reach the same message: only the [std::move] spelling
// is recognised as a move that bought nothing, while the cast is described by the type's lack
// of move operations. Both end up naming the reference remedy.
struct CastNoMove : BaseNoMove {
  explicit CastNoMove(NoMove p) : BaseNoMove(static_cast<NoMove&&>(p)) {}
};

void use_forward_no_move_bad() { ForwardNoMove w(NoMove("a")); }
void use_forward_has_move_bad() { ForwardHasMove w(HasMove("b")); }
void use_moved_has_move_ok() { MovedHasMove w(HasMove("c")); }
void use_cast_has_move_ok() { CastHasMove w(HasMove("d")); }
void use_moved_no_move_ok() { MovedNoMove w(NoMove("e")); }
void use_cast_no_move_ok() { CastNoMove w(NoMove("f")); }

} // namespace unnecessary_copy_unmovable
