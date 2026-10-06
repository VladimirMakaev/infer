This is an error reported on C++ and Objective C classes whenever:

- Some class method directly uses locking primitives (not transitively).
- It has a public method which writes to some member `x` while holding a lock.
- It has a public method which reads `x` without holding a lock.

The above may happen through a chain of calls. Above, `x` may also be a
container (an array, a vector, etc).

A method annotated with the clang thread safety attribute `requires_capability`
(eg `REQUIRES(mu)`), meaning that its callers must hold `mu`, is reported on as
if `mu` were held on entry, so its writes count as writes under a lock. Calls
to it are checked with the locks that the caller holds.

A function that is not defined in the analyzed code takes or releases the
capabilities named by its `acquire_capability`, `release_capability` or
`try_acquire_capability` attribute (eg `ACQUIRE(mu)`), including the constructor
and destructor of a `scoped_lockable` guard.

### Fixing Lock Consistency Violation reports

- Avoid the offending access (most often the read). Of course, this may not be
  possible.
- Use synchronization to protect the read, by using the same lock protecting the
  corresponding write.
- Make the method doing the read access private. This should silence the
  warning, since Infer looks for a pair of non-private methods. Objective-C:
  Infer considers a method as private if it's not exported in the header-file
  interface.
