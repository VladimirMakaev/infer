let is_running_unit_test = true

let pulse_sanity_checks = true

(* The opaque-payload caps, mirroring the --pulse-opaque-max-size / --pulse-opaque-max-depth
   defaults in infer/src/base/Config.ml. The VALUES matter, not just the names: over the cap
   PulseFormulaTerm drops the payload and substitutes a fresh unconstrained variable, so a shim
   with a smaller cap would make the properties exercise the collapse path far more often than
   the engine does, and a larger one would stop exercising it at all. Either way the harness
   would still be green while testing something other than the shipped behaviour. Keep these in
   step with Config.ml; shim-coverage.sh fails the build if a new Config.* appears and is not
   defined here, but it cannot tell that a number has drifted. *)
let pulse_opaque_max_size = 256

let pulse_opaque_max_depth = 32
