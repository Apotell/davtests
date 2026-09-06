/*
:name: chapter9_error_rules
:description: IEEE 1800-2023 Clause 9 (Processes) error scenarios
:tags: 9.2.2.2.2 9.3.2 9.4.2 9.6.2
*/

// catalog row 269 | 9.2.2.2.2 | COMP
// Statements in an always_comb (and, per 9.2.2.3, an always_latch) shall not
// include those that block, have blocking timing or event controls, or
// fork-join statements.
module r269_m;
  logic a, b, clk;
  always_comb begin
    @(posedge clk);
    #5 a = b;
    fork join
  end
endmodule

// catalog row 275 | 9.3.2 | COMP
// Within a fork-join_any or fork-join_none block it shall be illegal to refer
// to formal arguments passed by reference, other than in the initialization
// value expressions of variables declared in a block_item_declaration of the
// fork, unless the argument is declared ref static.
module r275_m;
  task r275_t(ref int r);
    fork
      r = 1;
    join_none
  endtask
endmodule

// catalog row 281 | 9.4.2 | COMP
// Event expressions shall return singular values. Aggregate types may be used
// in an event expression only if the expression reduces to a singular value.
module r281_m;
  typedef struct { int a; int b; } r281_s_t;
  r281_s_t s;
  logic x, y;
  always @(s) x = y;
endmodule

// catalog row 284 | 9.6.2 | COMP
// The disable statement can be used to disable named blocks within a function,
// but cannot be used to disable functions themselves.
module r284_m;
  function int f();
    return 0;
  endfunction
  initial disable f;
endmodule
