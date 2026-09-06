/*
:name: chapter35_error_rules
:description: IEEE 1800-2023 Clause 35 (DPI) error scenarios
:tags: 35.4
*/

// catalog row 1108 | 35.4 | COMP
// A global/linkage name shall follow C naming conventions: it shall start with
// a letter or underscore followed by alphanumeric characters or underscores.
// If given as an escaped identifier, the leading backslash and trailing
// whitespace are stripped and the result shall still comply with C identifier
// rules. Here the stripped linkage name is "init[1]", which does not.
module r1108_m;
  import "DPI-C" \init[1] = function void f();
endmodule
