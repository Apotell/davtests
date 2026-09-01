/*
:name: chapter7_error_rules_inv4
:description: IEEE 1800-2023 7.12.2 -- a with clause on the reverse() method
:tags: 7.12.2
*/

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
