#include <stdint.h>
int nondet(void);

/* The detector I intend to generate thousands of. The guard `v == R` is the probe:
   if inferbo's interval for v EXCLUDES R it prunes the branch and stays silent, which
   for an achievable R means it proved a false safety. These four calibrate that claim. */

/* C1 REACHABILITY CONTROL shape: unconditional access, must always report. */
int c1_reach(void) {
  int64_t v = nondet() ? 0 : 10;
  int a[1]; a[1] = 1;
  return a[0] + (int)v;
}

/* C2 FEASIBLE guard: 5 is inside [0,10], so the branch is live and this must REPORT.
   If it is silent, every "silent" verdict in the grid is meaningless. */
int c2_feasible(void) {
  int64_t v = nondet() ? 0 : 10;
  if (v == 5) { int a[1]; a[1] = 1; return a[0]; }
  return 0;
}

/* C3 INFEASIBLE guard: 100 is outside [0,10], so a sound AND precise analyser prunes
   it and stays silent. If this REPORTS, inferbo does not prune on ==, and silence can
   never be produced by exclusion -- the detector would have no discriminating power. */
int c3_infeasible(void) {
  int64_t v = nondet() ? 0 : 10;
  if (v == 100) { int a[1]; a[1] = 1; return a[0]; }
  return 0;
}

/* C4 the join is real: inferbo must widen {0,10} to [0,10], not keep two disjuncts.
   7 is in [0,10] but is NOT concretely achievable. A report here means the domain is
   an interval (expected). Silence would mean disjunctive precision, which would make
   "silent" ambiguous between exclusion and genuine infeasibility. */
int c4_interval_not_set(void) {
  int64_t v = nondet() ? 0 : 10;
  if (v == 7) { int a[1]; a[1] = 1; return a[0]; }
  return 0;
}
