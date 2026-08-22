/*
:name: chapter3_error_rules_inv2
:description: IEEE 1800-2023 3.11 -- a primitive cannot instantiate a building block
:tags: 3.11
:should_fail_because: primitives are leaves in the hierarchy and cannot instantiate anything
*/

// catalog row 3 | 3.11 | PARSE
// Primitives cannot instantiate other building blocks; they are leaves in the
// hierarchy tree. A primitive body may not contain module, interface, program,
// checker or primitive instances.
//
// Kept alone in its own fixture: the illegal instantiation desynchronizes the
// parser badly enough that it reports "mismatched input 'table' expecting
// <EOF>", i.e. everything after it in the same file is lost. Nothing may be
// appended below.

module r3_sub;
endmodule

primitive r3_p (out, in);
  output out;
  input in;
  r3_sub u1();
  table
    0 : 1;
    1 : 0;
  endtable
endprimitive
