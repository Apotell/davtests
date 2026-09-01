/*
:name: chapter7_error_rules_inv2
:description: IEEE 1800-2023 7.4.1 -- packed array of a non-single-bit type
:tags: 7.4.1
:should_fail_because: real is not a single-bit type, an enum, or a packed aggregate
*/

// catalog row 34 | 7.4.1 | COMP
// Packed arrays can be made of only the single-bit data types (bit, logic,
// reg), enumerated types, and recursively other packed arrays and packed
// structures.
//
// Kept in its own fixture: HLC's grammar does not accept a dimension
// directly after 'real' at all ("missing {...} at '['" then "extraneous
// input 'v'") -- 'real' has no bit-vector form for a trailing dimension to
// attach to, so this is a PARSE-level rejection, and the parser's recovery
// leaves a malformed typespec behind (a follow-on "invalid name" report on
// the orphaned RefTypespec). See Row34.
module r34_m;
  real [3:0] v; // illegal - packed array of real
endmodule
