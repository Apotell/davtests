/*
:name: chapter15_error_rules
:description: IEEE 1800-2023 Clause 15 error scenarios
:tags: 15.4.5
*/

// Every scenario below is derived from one row of the SV error catalog
// (docs/error_catalog.xml). The "catalog row N" comment is the link back to
// that row; the matching gtest is named RowN_...

// catalog row 460 | 15.4.5 | COMP
// The ref message argument of mailbox get()/try_get()/peek()/try_peek()
// shall be a valid left-hand expression.
module r460_m;
  mailbox mb = new();
  int a, b;
  initial mb.get(a + b); // illegal: message argument is not a valid left-hand expression
endmodule


// catalog row 460 | 15.4.6, 15.4.8 | COMP
// try_get() and try_peek() are functions, so their calls are method function
// calls; the same requirement holds for their ref message argument.
module r460_f_m;
  mailbox mb = new();
  int a, b, n;
  initial begin
    n = mb.try_get(a + b);  // illegal: message argument is not a valid left-hand expression
    n = mb.try_peek(a + b); // illegal: message argument is not a valid left-hand expression
  end
endmodule
