/*
:name: chapter16_error_rules
:description: IEEE 1800-2023 Clause 16 error scenarios
:tags: 16.6 16.8 16.8.1 16.9.4 16.13.6 16.14.7 16.16
*/

// Every scenario below is derived from one row of the SV error catalog
// (docs/error_catalog.xml). The "catalog row N" comment is the link back to
// that row; the matching gtest is named RowN_...

// catalog row 474 | 16.6 | LINT
// Expressions in concurrent assertions shall not reference non-static
// class properties or methods.
class r474_C;
  bit p;
  function bit get(); return p; endfunction
endclass
module r474_m(input logic clk);
  r474_C obj = new;
  a1: assert property (@(posedge clk) obj.p); // illegal: non-static class property referenced in an assertion
endmodule

// catalog row 477 | 16.6 | LINT
// Functions that appear in concurrent assertion expressions shall not
// contain output, inout, or ref arguments (const ref is allowed).
module r477_m(input logic clk, logic a);
  function bit f(output bit o); o = 1; return a; endfunction
  bit o;
  a1: assert property (@(posedge clk) f(o)); // illegal: function has an output argument
endmodule

// catalog row 478 | 16.6 | LINT
// Functions appearing in concurrent assertion expressions shall be
// automatic (or preserve no state information) and have no side effects.
module r478_m(input logic clk, logic a);
  function bit f();
    static int calls = 0; // preserves state across calls
    calls++;             // side effect
    return a;
  endfunction
  a1: assert property (@(posedge clk) f()); // illegal: function is not side-effect free
endmodule

// catalog row 476 | 16.6 | COMP
// Evaluation of an expression in a concurrent assertion shall not have any
// side effects (e.g., increment and decrement operators are not allowed);
// only sequence match items whose variable_lvalue is a local variable may
// use the C assignment, increment, and decrement operators.
module r476_m(input logic clk);
  int cnt;
  a1: assert property (@(posedge clk) (cnt++ < 4)); // illegal: side effect in an assertion expression
endmodule

// catalog row 484 | 16.8 | LINT
// If the terminal $ is an actual argument of a named sequence instance,
// the corresponding formal argument shall be untyped and each of its
// references shall either be an upper bound in a
// cycle_delay_const_range_expression or itself be an actual argument in an
// instance of a named sequence.
module r484_m(input logic clk, logic a, b);
  sequence s(x, y); x ##1 y; endsequence // y is referenced as an ordinary operand
  a1: assert property (@(posedge clk) s(a, $)); // illegal: $ bound to a formal not used as a range upper bound
endmodule

// catalog row 483 | 16.8 | COMP
// An instance of a named sequence shall provide an actual argument in the
// list of arguments for each formal argument that does not have a default
// actual argument declared.
module r483_m(input logic clk, logic a, b);
  sequence s(x, y); x ##1 y; endsequence
  a1: assert property (@(posedge clk) s(a)); // illegal: no actual argument for formal y, which has no default
endmodule

// catalog row 489 | 16.8.1 | LINT
// If a formal argument is of type sequence, the actual argument shall be a
// sequence_expr, and each reference to the formal shall be in a place
// where a sequence_expr is legal or as an operand of the sequence methods
// triggered or matched.
module r489_m(input logic clk, logic a, b);
  sequence s(sequence q); q[->2]; endsequence // illegal: sequence-typed formal used as the operand of goto repetition
  a1: assert property (@(posedge clk) s(a ##1 b));
endmodule

// catalog row 490 | 16.8.1 | LINT
// If a formal argument is of type event, the actual argument shall be an
// event_expression and each reference to the formal shall be in a place
// where an event_expression may be written.
module r490_m(input logic clk, logic a);
  sequence s(event ev); @(ev) a; endsequence
  a1: assert property (s(clk)); // illegal: clk is not an event_expression (use posedge clk)
endmodule

// catalog row 514 | 16.9.4 | LINT
// The global clocking future sampled value functions shall not be used in
// assertions containing sequence match items.
module r514_m(input logic clk, logic a, b, c);
  global clocking @(posedge clk); endclocking
  sequence s;
    bit v;
    (a, v = a) ##1 (b == v)[->1];
  endsequence
  a1: assert property (@(posedge clk) s |=> $future_gclk(c)); // illegal: future function in an assertion with match items
endmodule

// catalog row 513 | 16.9.4 | COMP
// The global clocking future sampled value functions shall not be nested.
module r513_m(input logic clk, logic a, b);
  global clocking @(posedge clk); endclocking
  a1: assert property (@(posedge clk) $future_gclk(a || $rising_gclk(b))); // illegal: nested future sampled value functions
endmodule

// catalog row 512 | 16.9.4 | LINT
// The global clocking future sampled value functions may be invoked only
// in a property_expr or a sequence_expr; in particular they shall not be
// used in assertion action blocks.
module r512_m(input logic clk, logic a, sig);
  global clocking @(posedge clk); endclocking
  a1: assert property (@(posedge clk) a)
    else $error("%b", $future_gclk(sig)); // illegal: future sampled value function in an action block
endmodule

// catalog row 511 | 16.9.4 | ELAB
// The global clocking past and future sampled value functions may be used
// only if global clocking is defined by a global clocking declaration.
module r511_m(input logic clk, logic a);
  // no `global clocking @(posedge clk); endclocking` declared anywhere
  a1: assert property (@(posedge clk) $rose_gclk(a)); // illegal: no global clocking defined
endmodule

// catalog row 554 | 16.13.6 | LINT
// The matched method can only be used in sequence expressions.
module r554_m(input logic clk, sysclk, logic a, b);
  sequence e1; @(posedge clk) a ##1 b; endsequence
  initial begin
    wait (e1.matched); // illegal: matched used outside a sequence expression
  end
endmodule

// catalog row 560 | 16.14.7 | COMP
// An inferred clocking or disable function ($inferred_clock,
// $inferred_disable) shall only be used as the entire default value
// expression for a formal argument to a property, sequence, or checker
// declaration.
module r560_m(input logic clk, logic a, b, rst);
  default clocking cb @(posedge clk); endclocking
  property p(start_event, form);
    @($inferred_clock) start_event |=> form; // illegal: not used as an entire formal default value expression
  endproperty
  a1: assert property (p(a, b));
endmodule

// catalog row 563 | 16.16 | COMP
// No explicit clocking event is allowed in any property or sequence
// declaration within a clocking block; all such declarations take the
// clocking block's clocking event as their leading clocking event.
module r563_m(input logic clk, logic b);
  default clocking posedge_clk @(posedge clk);
    sequence s1;
      @(posedge clk) b[*3]; // illegal: explicit clocking event in a clocking block declaration
    endsequence
  endclocking
endmodule
