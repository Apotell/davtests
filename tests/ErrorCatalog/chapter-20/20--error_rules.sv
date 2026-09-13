/*
:name: chapter20_error_rules
:description: IEEE 1800-2023 Clause 20 error scenarios
:tags: 20.2 20.4.3 20.6.3 20.11 20.14.1
*/

// Every scenario below is derived from one row of the SV error catalog
// (docs/error_catalog.xml, sheet "Error Catalog"). The "catalog row N"
// comment is the link back to that row; the matching gtest is named RowN_...
//
// This file merges every catalog-20 scenario supplied by the colleague's
// DUT files (dut_ch_20_2.sv, dut_ch_20_4_3.sv, dut_ch_20_6_3.sv,
// dut_ch_20_11.sv, dut_ch_20_14_1.sv) into a single compilation. Every DUT
// source declares its scenario as "module m;", so each top-level module
// below carries an r<N>_ prefix (N = catalog row) to stay unique in this
// merged file.

// catalog row 731 | 20.2 | COMP
// The optional argument to $stop and $finish shall be an expression whose
// value is 0, 1, or 2 (Table 20-1); any other diagnostic-level value is
// illegal.
module r731_m;
  initial begin
    $finish(3); // ILLEGAL: diagnostic argument must be 0, 1, or 2
  end
endmodule

// catalog row 732 | 20.4.3 | COMP
// The units_number and precision_number arguments of $timeformat shall be
// integers in the range from 2 down to -15.
module r732_m;
  initial $timeformat(-20, 5, " ns", 10); // ILLEGAL: units_number outside 2..-15
endmodule

// catalog row 737 | 20.6.3 | COMP
// The argument to $isunbounded shall be a parameter name (a
// ps_parameter_identifier or hierarchical_parameter_identifier); an ordinary
// variable or expression is not allowed.
module r737_m;
  int v;
  bit b;
  initial b = $isunbounded(v); // ILLEGAL: argument must be a parameter name
endmodule

// catalog row 744 | 20.11 | COMP
// The control_type argument of $assertcontrol shall be an integer expression
// whose value is one of the values defined in Table 20-5 (1 through 11).
module r744_m;
  initial $assertcontrol(99); // ILLEGAL: control_type must be one of 1..11
endmodule

// catalog row 746 | 20.14.1 | COMP
// The seed argument of $random shall be an integral variable (not a
// constant, expression, or non-integral object), because it is updated by
// the call.
module r746_m;
  int r;
  initial r = $random(3); // ILLEGAL: seed must be an integral variable
endmodule
