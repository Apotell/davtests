/*
:name: chapter7_error_rules
:description: IEEE 1800-2023 Clause 7 error scenarios that parse cleanly
:tags: 7.3.1 7.4.5 7.7 7.8.1 7.12.2
*/

// Every scenario below is derived from one row of the SV error catalog
// (filtered set). The "catalog row N" comment is the link back to that row;
// the matching gtest is named RowN_...
//
// This file holds only the scenarios that parse without a syntax error, so a
// single compilation observes all of them at once. Rows 34, 36 and 40 raise
// syntax/internal errors and live in the sibling _inv*.sv fixtures instead --
// see those files for why each is kept separate.

// catalog row 33 | 7.3.1 | COMP
// When the packed qualifier is used without the soft qualifier on an
// untagged union (hard packed), all members of that union shall be the same
// size.
typedef union packed { bit [7:0] a; bit [3:0] b; } r33_u_t; // illegal - members differ in size

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

// catalog row 35 | 7.4.5 | COMP
// Slices of an array can only be applied to one dimension; other dimensions
// may only have single index values in the expression.
module r35_m;
  int r35_a[0:7][0:7], r35_b[0:1][0:1];
  initial r35_b = r35_a[0:1][0:1]; // illegal - slice applied to more than one dimension
endmodule

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

// catalog row 37 | 7.7 | COMP
// A dynamic array or queue shall not be passed as an actual argument if the
// DPI formal argument has unsized dimensions and an output direction mode.
import "DPI-C" function void r37_f(output int arr[]);
module r37_m;
  int r37_q[$];
  initial r37_f(r37_q); // illegal - queue passed to an unsized output DPI open array formal
endmodule

// catalog row 38 | 7.8.1 | COMP
// For an associative array with a wildcard index type, nonintegral index
// values are illegal and shall result in an error.
module r38_m;
  int r38_aa[*];
  real r38_r = 1.0;
  int r38_v;
  initial r38_v = r38_aa[r38_r]; // illegal - nonintegral index into a wildcard-index array
endmodule

// catalog row 39 | 7.8.1 | COMP
// Associative arrays that specify a wildcard index type shall not be used in
// a foreach loop, nor with an array manipulation method that returns an
// index value or an array of index values.
//
// NOTE: the catalog's own source used a bare ';' as the foreach body
// ("foreach (aa[i]) ;"), which HLC's grammar rejects on its own (a syntax
// error unrelated to the wildcard-index rule this row tests); rewritten
// below with an empty begin/end block so the wildcard-index use is the only
// thing under test.
module r39_m;
  int r39_aa[*];
  initial foreach (r39_aa[i]) begin end // illegal - foreach over a wildcard-index associative array
endmodule

// catalog row 40 | 7.12.2 | COMP
// Specifying a with clause on the reverse() array ordering method shall be a
// compiler error.
//
// Kept in its own fixture: HLC does not model 'with' on reverse() at all --
// the call resolves to a MethodFuncCall with no actual, which the linter
// separately reports as "Null Actual" (LN7705). Isolated so that internal
// diagnostic does not appear alongside unrelated rows.
module r40_m;
  int r40_q[$] = {1, 2, 3};
  initial r40_q.reverse with (item); // illegal - with clause on reverse()
endmodule
