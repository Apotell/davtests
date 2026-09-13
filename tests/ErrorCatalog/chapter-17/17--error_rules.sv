/*
:name: chapter17_error_rules
:description: IEEE 1800-2023 Clause 17 error scenarios
:tags: 17.3 17.8
*/

// Every scenario below is derived from one row of the SV error catalog
// (docs/error_catalog.xml). The "catalog row N" comment is the link back to
// that row; the matching gtest is named RowN_...
//
// All four scenarios here are semantic (COMP/LINT) violations that parse
// cleanly, so one compilation observes all of them at once -- no sibling
// _inv*.sv fixture is needed for this chapter.

// catalog row 582 | 17.3 | COMP
// Each checker actual output argument shall be a variable_lvalue or a
// net_lvalue.
checker r582_c(bit r582_a, output bit r582_fail); endchecker
module r582_m;
  bit r582_x, r582_y, r582_z;
  r582_c r582_inst(r582_x, r582_y & r582_z); // ILLEGAL: actual output argument is not a variable_lvalue or net_lvalue
endmodule

// catalog row 580 | 17.3 | LINT
// If $ is an actual input argument to a checker instance, the corresponding
// formal shall be untyped and each of its references shall either be an
// upper bound in a cycle_delay_const_range_expression or itself be an
// actual argument in an instance of a named sequence or property, in a
// checker instance, or as a default argument to a nested checker.
checker r580_c(logic r580_hi, event r580_clk);   // formal is typed, and used as a plain operand
  r580_a1: assert property (@r580_clk r580_hi);
endchecker
module r580_m;
  logic r580_msig;
  r580_c r580_inst($, posedge r580_msig); // ILLEGAL: $ passed to a typed formal used outside a delay range bound
endmodule

// catalog row 596 | 17.8 | COMP
// Functions appearing in expressions on the right-hand side of checker
// variable assignments shall not contain output, inout, or ref arguments
// (const ref is allowed).
checker r596_c(bit r596_a, event r596_clk);
  bit r596_z;
  function bit r596_f(output bit r596_o); // has an output argument
    r596_o = 1'b0;
    return 1'b1;
  endfunction
  bit r596_t;
  always_ff @r596_clk
    r596_z <= r596_f(r596_t); // ILLEGAL: function with an output argument in a checker variable assignment
endchecker

// catalog row 597 | 17.8 | COMP
// Functions called in expressions on the right-hand side of checker
// variable assignments shall be automatic (or preserve no state
// information) and have no side effects.
checker r597_c(bit r597_a, event r597_clk);
  bit r597_z;
  function bit r597_f(bit r597_p);
    static int r597_cnt = 0; // static state: function is not automatic and has a side effect
    r597_cnt++;
    return r597_p;
  endfunction
  always_ff @r597_clk
    r597_z <= r597_f(r597_a); // ILLEGAL: function retains state / has side effects
endchecker
