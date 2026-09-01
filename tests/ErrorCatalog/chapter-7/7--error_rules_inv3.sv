/*
:name: chapter7_error_rules_inv3
:description: IEEE 1800-2023 7.5 -- new[] constructor on a non-dynamic array
:tags: 7.5
:should_fail_because: the leftmost unpacked dimension of arr3 is fixed-size, not dynamic
*/

// catalog row 36 | 7.5 | COMP
// For an identifier to represent a dynamic array it shall be declared with a
// dynamic array dimension as the leftmost unpacked dimension; using the
// new[] constructor on an array that is not itself a dynamic array is an
// error.
//
// Kept in its own fixture: HLC's grammar does not accept 'new [4]' as an
// initializer in this multi-dimensional declaration shape at all
// ("mismatched input 'new' expecting <expression>") -- a PARSE-level
// rejection rather than the semantic one this row names. See Row36.
module r36_m;
  int r36_arr3 [1][2][] = new [4]; // illegal - arr3 is not a dynamic array
endmodule
