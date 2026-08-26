/*
:name: chapter16_error_rules
:description: IEEE 1800-2023 Clause 16 (Assertions) error scenarios
:tags: 16.4 16.6 16.10
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

// catalog row 476 | 16.6 | COMP
// Evaluation of an expression in a concurrent assertion shall not have any
// side effects (e.g., increment and decrement operators are not allowed); only
// sequence match items whose variable_lvalue is a local variable may use the C
// assignment, increment, and decrement operators.
module r476_m (input logic clk);
  int cnt;
  r476_a1: assert property (@(posedge clk) (cnt++ < 4));
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
