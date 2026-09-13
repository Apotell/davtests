/*
:name: chapter35_error_rules
:description: IEEE 1800-2023 Clause 35 error scenarios
:tags: 35.4 35.5.4
*/

// Every scenario below is derived from one row of the SV error catalog
// (docs/error_catalog.xml). The "catalog row N" comment is the link back to
// that row; the matching gtest is named RowN_...
//
// This file holds only the scenarios that parse without a syntax error, so
// a single compilation observes both of them at once. The other two rows
// from this chapter (1108, 1136) each desynchronize HLC's parser badly
// enough that the whole enclosing design element is lost, so they live in
// their own sibling fixtures, compiled independently in the same run.

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
