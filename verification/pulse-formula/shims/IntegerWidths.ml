type t = {char_width: int; short_width: int; int_width: int; long_width: int; longlong_width: int}

let range_of_ikind _widths (_k : Typ.ikind) = (Z.of_int (-2147483648), Z.of_int 2147483647)
