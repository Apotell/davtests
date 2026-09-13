/*
:name: chapter35_error_rules
:description: IEEE 1800-2023 Clause 35 (DPI) error scenarios
:tags: 35.4 35.5.4
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

// catalog row 1109 | 35.4 | LINT
// All declarations using the same c_identifier shall be declared with the
// same DPI version string; mixing the deprecated "DPI" and the current
// "DPI-C" spec strings for one c_identifier is illegal.
module r1109_m;
  import "DPI-C" function int r1109_f(int i);
endmodule
module r1109_n;
  import "DPI" function int r1109_f(int i); // ERROR: same c_identifier declared with a different DPI version string
endmodule

// catalog row 1123 | 35.5.4 | COMP
// Use of the deprecated dpi_spec_string "DPI" shall generate a
// compile-time warning or error stating that "DPI" is deprecated and
// should be replaced with "DPI-C", and that use of "DPI-C" may require
// changes in the application's C code.
module r1123_m;
  import "DPI" function int r1123_f(int i); // warning/error: "DPI" is deprecated, use "DPI-C"
endmodule
