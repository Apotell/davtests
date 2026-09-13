/*
:name: chapter31_error_rules
:description: IEEE 1800-2023 Clause 31 error scenarios
:tags: 31.3.1 31.3.2 31.3.3 31.3.4 31.3.5 31.3.6 31.4.1 31.4.2 31.4.3 31.4.4 31.4.5 31.9.1
*/

// Every scenario below is derived from one row of the SV error catalog
// (docs/error_catalog.xml). The "catalog row N" comment is the link back to
// that row; the matching gtest is named RowN_...
//
// All twelve scenarios are timing-check limit-value violations inside a
// specify block (Category ELAB in the catalog, not PARSE), so none of them
// raises a syntax error and all are observed by a single compilation.
//
// A single `timescale is applied once, near the top, before any scenario --
// `timescale itself has file-wide/until-changed scope in SV, not module
// scope, so it cannot be varied per module within one file. Rows 1034 and
// 1037 need a unit of time precision in scope for their
// "limit1 + limit2 > precision" arithmetic to be meaningful; every other
// row only checks that its own limit is non-negative and is unaffected by
// which precision is in effect. 1ns/1ns keeps both paired-limit sums
// (-10ns+5ns=-5ns for row 1034, -8ns+4ns=-4ns for row 1037) well below the
// 1ns precision, so the intended violation is still exercised.

`timescale 1ns/1ns

// catalog row 1032 | 31.3.1 | ELAB
// The $setup timing check limit shall be a non-negative constant
// expression; a negative setup limit is illegal for $setup (negative
// values are only permitted for $setuphold and $recrem).
module r1032_m (input clk, input d);
  specify
    $setup(d, posedge clk, -5);  // ILLEGAL: $setup limit must be non-negative
  endspecify
endmodule

// catalog row 1033 | 31.3.2 | ELAB
// The $hold timing check limit shall be a non-negative constant
// expression; a negative hold limit is illegal for $hold.
module r1033_m (input clk, input d);
  specify
    $hold(posedge clk, d, -1);  // ILLEGAL: $hold limit must be non-negative
  endspecify
endmodule

// catalog row 1034 | 31.3.3 | ELAB
// When either the setup limit or the hold limit of a $setuphold check is
// negative, the restriction setup_limit + hold_limit > (simulation unit of
// precision) shall hold. A negative-limit pair whose sum is not greater
// than one unit of precision is illegal.
module r1034_m (input clk, input d);
  specify
    $setuphold(posedge clk, d, -10, 5);  // ILLEGAL: setup_limit + hold_limit = -5, not > precision
  endspecify
endmodule

// catalog row 1035 | 31.3.4 | ELAB
// The $removal timing check limit shall be a non-negative constant
// expression.
module r1035_m (input clr, input clk);
  specify
    $removal(posedge clr, posedge clk, -3);  // ILLEGAL: $removal limit must be non-negative
  endspecify
endmodule

// catalog row 1036 | 31.3.5 | ELAB
// The $recovery timing check limit shall be a non-negative constant
// expression.
module r1036_m (input clr, input clk);
  specify
    $recovery(posedge clr, posedge clk, -3);  // ILLEGAL: $recovery limit must be non-negative
  endspecify
endmodule

// catalog row 1037 | 31.3.6 | ELAB
// When either the removal limit or the recovery limit of a $recrem check is
// negative, the restriction removal_limit + recovery_limit > (simulation
// unit of precision) shall hold.
module r1037_m (input clr, input clk);
  specify
    $recrem(posedge clr, posedge clk, -8, 4);  // ILLEGAL: recovery_limit + removal_limit = -4, not > precision
  endspecify
endmodule

// catalog row 1038 | 31.4.1 | ELAB
// The $skew timing check limit shall be a non-negative constant
// expression.
module r1038_m (input cp, input cpn);
  specify
    $skew(posedge cp, negedge cpn, -50);  // ILLEGAL: $skew limit must be non-negative
  endspecify
endmodule

// catalog row 1039 | 31.4.2 | ELAB
// The $timeskew timing check limit shall be a non-negative constant
// expression.
module r1039_m (input cp, input cpn);
  specify
    $timeskew(posedge cp, negedge cpn, -50);  // ILLEGAL: $timeskew limit must be non-negative
  endspecify
endmodule

// catalog row 1040 | 31.4.3 | ELAB
// Both $fullskew timing check limits (limit1 and limit2) shall be
// non-negative constant expressions.
module r1040_m (input cp, input cpn);
  specify
    $fullskew(posedge cp, negedge cpn, 50, -70);  // ILLEGAL: $fullskew limit2 must be non-negative
  endspecify
endmodule

// catalog row 1044 | 31.4.4 | ELAB
// The $width threshold argument shall be a non-negative constant
// expression.
module r1044_m (input clr);
  specify
    $width(negedge clr, 6, -1);  // ILLEGAL: $width threshold must be non-negative
  endspecify
endmodule

// catalog row 1043 | 31.4.4 | ELAB
// The $width timing check limit shall be a non-negative constant
// expression.
module r1043_m (input clr);
  specify
    $width(negedge clr, -6, 0);  // ILLEGAL: $width limit must be non-negative
  endspecify
endmodule

// catalog row 1046 | 31.4.5 | ELAB
// The $period timing check limit shall be a non-negative constant
// expression.
module r1046_m (input clk);
  specify
    $period(posedge clk, -25);  // ILLEGAL: $period limit must be non-negative
  endspecify
endmodule

// catalog row 1052 | 31.9.1 | ELAB
// When delayed signals are used for negative timing checks and the
// adjusted limit of a $setup/$hold/$setuphold/$recovery/$removal/$recrem/
// $width/$period/$nochange check becomes less than or equal to 0, the
// limit shall be set to 0 and the simulator shall issue a warning.
module r1052_m (input clk, input d);
  reg notif;
  specify
    // warning expected: adjusted limit for the delayed signals collapses to <= 0 and is clamped to 0
    $setuphold(posedge clk, d, -20, 5, notif);
  endspecify
endmodule
