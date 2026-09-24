/*
:name: chapter17_error_rules
:description: IEEE 1800-2023 Clause 17 (Checkers) error scenarios
:tags: 17.2 17.3 17.7 17.8
*/

// catalog row 573 | 17.2 | COMP
// Modules, interfaces, and programs shall not be instantiated inside checkers.
module r573_sub;
endmodule

checker r573_c;
  r573_sub s1();
endchecker

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

// catalog row 582 | 17.3 | COMP
// Each checker actual output argument shall be a variable_lvalue or a
// net_lvalue.
checker r582_c(bit r582_a, output bit r582_fail); endchecker
module r582_m;
  bit r582_x, r582_y, r582_z;
  r582_c r582_inst(r582_x, r582_y & r582_z); // ILLEGAL: actual output argument is not a variable_lvalue or net_lvalue
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

// catalog row 590 | 17.7 | COMP
// All variables defined in a checker body shall have static lifetimes; an
// automatic-lifetime checker variable is illegal.
checker r590_c(bit r590_a);
  automatic bit r590_v; // illegal: checker variables shall have static lifetime
endchecker

// catalog row 595 | 17.8 | COMP
// The formal arguments and internal variables of functions used in
// checkers shall not be declared as free (rand) variables.
checker r595_c(bit r595_a);
  function bit r595_f(rand bit r595_p); // illegal: function formal declared as a free variable
    rand bit r595_local_v;              // illegal: function internal variable declared as a free variable
    return r595_p;
  endfunction
endchecker
