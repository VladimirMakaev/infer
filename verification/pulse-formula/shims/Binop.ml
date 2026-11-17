type ikind_opt = unit

type t =
  | PlusA of ikind_opt option
  | PlusPI
  | MinusA of ikind_opt option
  | MinusPI
  | MinusPP
  | Mult of ikind_opt option
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
  | LOr
