/*
:name: chapter16_error_rules
:description: IEEE 1800-2023 Clause 16 (Assertions) error scenarios
:tags: 16.4 16.6 16.8 16.8.1 16.8.2 16.9.4 16.10 16.12.19 16.13.6 16.14.7 16.16
*/

// catalog row 468 | 16.4 | COMP
// The pass and fail statements in a deferred immediate assertion's
// action_block shall each consist of a single subroutine call; a begin-end
// block is not a subroutine call and is therefore illegal.
module r468_m (input logic a, b);
  always_comb begin
    r468_a1: assert #0 (a == b) else begin
      $error("mismatch");
    end
  end
endmodule

// catalog row 474 | 16.6 | LINT
// Expressions in concurrent assertions shall not reference non-static
// class properties or methods.
class r474_c;
  bit p;
  function bit get(); return p; endfunction
endclass
module r474_m (input logic clk);
  r474_c obj = new;
  r474_a1: assert property (@(posedge clk) obj.p);
endmodule

// catalog row 476 | 16.6 | COMP
// Evaluation of an expression in a concurrent assertion shall not have any
// side effects (e.g., increment and decrement operators are not allowed); only
// sequence match items whose variable_lvalue is a local variable may use the C
// assignment, increment, and decrement operators.
module r476_m (input logic clk);
  int cnt;
  r476_a1: assert property (@(posedge clk) (cnt++ < 4));
endmodule

// catalog row 477 | 16.6 | LINT
// Functions that appear in concurrent assertion expressions shall not
// contain output, inout, or ref arguments (const ref is allowed).
module r477_m (input logic clk, input logic a);
  function bit f(output bit o); o = 1; return a; endfunction
  bit o;
  r477_a1: assert property (@(posedge clk) f(o));
endmodule

// catalog row 478 | 16.6 | LINT
// Functions appearing in concurrent assertion expressions shall be
// automatic (or preserve no state information) and have no side effects.
module r478_m (input logic clk, input logic a);
  function bit f();
    static int calls = 0;
    calls++;
    return a;
  endfunction
  r478_a1: assert property (@(posedge clk) f());
endmodule

// catalog row 483 | 16.8 | COMP
// An instance of a named sequence shall provide an actual argument in the
// list of arguments for each formal argument that does not have a default
// actual argument declared.
module r483_m (input logic clk, logic a, b);
  sequence r483_s(x, y); x ##1 y; endsequence
  r483_a1: assert property (@(posedge clk) r483_s(a));
endmodule

// catalog row 484 | 16.8 | LINT
// If the terminal $ is an actual argument of a named sequence instance,
// the corresponding formal argument shall be untyped and each of its
// references shall either be an upper bound in a
// cycle_delay_const_range_expression or itself be an actual argument in an
// instance of a named sequence.
module r484_m (input logic clk, logic a, b);
  sequence r484_s(x, y); x ##1 y; endsequence
  r484_a1: assert property (@(posedge clk) r484_s(a, $));
endmodule

// catalog row 489 | 16.8.1 | LINT
// If a formal argument is of type sequence, the actual argument shall be a
// sequence_expr, and each reference to the formal shall be in a place
// where a sequence_expr is legal or as an operand of the sequence methods
// triggered or matched.
module r489_m (input logic clk, logic a, b);
  sequence r489_s(sequence q); q[->2]; endsequence
  r489_a1: assert property (@(posedge clk) r489_s(a ##1 b));
endmodule

// catalog row 490 | 16.8.1 | LINT
// If a formal argument is of type event, the actual argument shall be an
// event_expression and each reference to the formal shall be in a place
// where an event_expression may be written.
module r490_m (input logic clk, logic a);
  sequence r490_s(event ev); @(ev) a; endsequence
  r490_a1: assert property (r490_s(clk));
endmodule

// catalog row 511 | 16.9.4 | ELAB
// The global clocking past and future sampled value functions may be used
// only if global clocking is defined by a global clocking declaration.
module r511_m (input logic clk, logic a);
  r511_a1: assert property (@(posedge clk) $rose_gclk(a));
endmodule

// catalog row 512 | 16.9.4 | LINT
// The global clocking future sampled value functions may be invoked only
// in a property_expr or a sequence_expr; in particular they shall not be
// used in assertion action blocks.
module r512_m (input logic clk, logic a, sig);
  global clocking @(posedge clk); endclocking
  r512_a1: assert property (@(posedge clk) a)
    else $error("%b", $future_gclk(sig));
endmodule

// catalog row 513 | 16.9.4 | COMP
// The global clocking future sampled value functions shall not be nested.
module r513_m (input logic clk, logic a, b);
  global clocking @(posedge clk); endclocking
  r513_a1: assert property (@(posedge clk) $future_gclk(a || $rising_gclk(b)));
endmodule

// catalog row 514 | 16.9.4 | LINT
// The global clocking future sampled value functions shall not be used in
// assertions containing sequence match items.
module r514_m (input logic clk, logic a, b, c);
  global clocking @(posedge clk); endclocking
  sequence r514_s;
    bit v;
    (a, v = a) ##1 (b == v)[->1];
  endsequence
  r514_a1: assert property (@(posedge clk) r514_s |=> $future_gclk(c));
endmodule

// catalog row 515 | 16.10 | COMP
// The data type of an assertion variable declaration shall be specified
// explicitly and shall be one of the types allowed within assertions as
// defined in 16.6.
module r515_m (input logic clk, logic a, b);
  sequence r515_s;
    chandle h;
    a ##1 b;
  endsequence
  r515_a1: assert property (@(posedge clk) r515_s);
endmodule

// catalog row 554 | 16.13.6 | LINT
// The matched method can only be used in sequence expressions.
module r554_m (input logic clk, sysclk, logic a, b);
  sequence r554_e1; @(posedge clk) a ##1 b; endsequence
  initial begin
    wait (r554_e1.matched);
  end
endmodule

// catalog row 560 | 16.14.7 | COMP
// An inferred clocking or disable function ($inferred_clock,
// $inferred_disable) shall only be used as the entire default value
// expression for a formal argument to a property, sequence, or checker
// declaration.
module r560_m (input logic clk, logic a, b, rst);
  default clocking r560_cb @(posedge clk); endclocking
  property r560_p(start_event, form);
    @($inferred_clock) start_event |=> form;
  endproperty
  r560_a1: assert property (r560_p(a, b));
endmodule

// catalog row 563 | 16.16 | COMP
// No explicit clocking event is allowed in any property or sequence
// declaration within a clocking block; all such declarations take the
// clocking block's clocking event as their leading clocking event.
module r563_m (input logic clk, logic b);
  default clocking r563_posedge_clk @(posedge clk);
    sequence r563_s1;
      @(posedge clk) b[*3];
    endsequence
  endclocking
endmodule

// catalog row 496 | 16.8.2 | COMP
// If one of the directions input, inout, or output is specified in a
// sequence port item, then the keyword local shall also be specified in
// that port item.
module r496_m (input logic clk, logic a);
  sequence r496_s(output logic v); // illegal: a direction requires the local keyword
    (a, v = a);
  endsequence
  logic r496_w;
  r496_a1: assert property (@(posedge clk) r496_s(r496_w));
endmodule

// catalog row 544 | 16.12.19 | COMP
// A local variable formal argument of a named property shall have direction
// input; it shall be illegal to declare a local variable formal argument of
// a named property with direction inout or output.
module r544_m (input logic clk, logic a, b, int data);
  property r544_p(local output int lv); (a, lv = data) |=> b; endproperty // illegal: output local variable formal in a property
  int r544_v;
  r544_a1: assert property (@(posedge clk) r544_p(r544_v));
endmodule
