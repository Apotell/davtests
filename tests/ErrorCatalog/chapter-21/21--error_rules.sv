/*
:name: chapter21_error_rules
:description: IEEE 1800-2023 Clause 21 (Input/output system tasks) error scenarios
:tags: 21.2.1 21.2.1.1 21.3.1 21.3.3 21.7.2 21.7.3.1 21.7.3.4
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

// catalog row 762 | 21.3.1 | COMP
// The type argument of $fopen shall contain a character string of one of the
// forms listed in Table 21-6 (r, rb, w, wb, a, ab, r+, r+b, rb+, w+, w+b,
// wb+, a+, a+b, ab+).
module r762_m;
  int fd;
  initial fd = $fopen("f.txt", "q"); // ILLEGAL: "q" is not a valid open type
endmodule

// catalog row 765 | 21.3.3 | COMP
// If not enough arguments are supplied for the format specifiers in
// $sformat's format_string, or too many are supplied, a warning shall be
// issued (and execution continues); an implementation may statically detect
// the mismatch and issue a compile-time error.
module r765_m;
  string s;
  initial $sformat(s, "%d %d", 1); // WARNING: too few arguments for the format
                                   // specifiers in the format string
endmodule

// catalog row 779 | 21.7.2 | COMP
// The VCD format provides no mechanism to dump part of a vector or to dump
// an expression; only whole variables and module scopes may be named as
// $dumpvars arguments.
module r779_m;
  logic [15:0] v;
  initial $dumpvars(0, v[8:15]); // ILLEGAL: a part-select of a vector cannot be dumped
endmodule

// catalog row 780 | 21.7.3.1 | COMP
// The scope_list argument of $dumpports shall consist only of
// module_identifiers; variables are not allowed.
module r780_m;
  logic v;
  initial $dumpports(v, "ports.vcd"); // ILLEGAL: only modules are allowed in scope_list
endmodule

// catalog row 781 | 21.7.3.1 | PARSE
// String literals are not allowed for the module_identifier entries of the
// $dumpports scope_list.
module r781_m;
  initial $dumpports("testbench.DUT", "ports.vcd"); // ILLEGAL: module identifier given
                                                    // as a string literal
endmodule

// catalog row 782 | 21.7.3.1 | PARSE
// If the first argument of $dumpports is null, a comma shall still be used
// before specifying the second argument.
module r782_m;
  initial $dumpports("ports.vcd"); // ILLEGAL: omitted scope_list requires a leading
                                   // comma, i.e. $dumpports(, "ports.vcd");
endmodule

// catalog row 786 | 21.7.3.4 | PARSE
// The filesize integer argument of $dumpportslimit is required; it may not
// be omitted.
module r786_m;
  initial $dumpportslimit(, "ports.vcd"); // ILLEGAL: filesize argument is required
endmodule
