/*
:name: chapter21_error_rules
:description: IEEE 1800-2023 Clause 21 (Input/output system tasks) error scenarios
:tags: 21.2.1 21.2.1.1
*/

// catalog row 754 | 21.2.1 | COMP
// For each % character (except %m, %l, and %%) that appears in a string
// literal argument of a display/write task, a corresponding expression
// argument shall be supplied after the string literal.
module r754_m;
  int a;
  initial $display("a=%d b=%d", a);
endmodule

// catalog row 755 | 21.2.1.1 | COMP
// It shall be an error if an undefined format specifier appears in a string
// literal argument.
module r755_m;
  int a;
  initial $display("a=%q", a);
endmodule
