/*
:name: chapter13_error_rules
:description: IEEE 1800-2023 Clause 13 (Tasks and functions) error scenarios
:tags: 13.4 13.4.1 13.4.3 13.4.4 13.5 13.5.3
*/

// catalog row 402 | 13.4 | COMP
// It shall be illegal to call a function with output, inout or (non-const)
// ref arguments in an event expression, in an expression within a procedural
// continuous assignment, or in an expression that is not within a procedural
// statement.
module r402_m;
  int a, b;
  wire w;
  function int f(output int o);
    o = 1;
    return 2;
  endfunction
  assign w = f(a);
  always @(f(a)) ;
endmodule

// catalog row 404 | 13.4.1 | COMP
// Calling a nonvoid function as if it had no return value shall be legal but
// shall issue a warning; casting the call to void suppresses the warning. The
// void'(f()) call below must stay silent -- it is the negative half of the
// rule and belongs in the same fixture as the positive half.
module r404_m;
  function int f();
    return 1;
  endfunction
  initial begin
    f();
    void'(f());
  end
endmodule

// catalog row 408 | 13.4.3 | COMP
// A constant function shall not have output, inout or ref arguments.
module r408_m;
  function integer cf(input [31:0] v, output [31:0] o);
    o = v;
    cf = v;
  endfunction
  logic [31:0] junk;
  localparam P = cf(8, junk);
endmodule

// catalog row 420 | 13.4.4 | LINT
// Calling a function that schedules an event that cannot become active until
// after the function returns is allowed only when the calling thread was
// created by an initial procedure, an always procedure, or a fork from one of
// those, and in a context where a side effect is allowed; implementations
// shall issue an error at compile time or run time otherwise.
class r420_IntClass; int a; endclass
module r420_m;
  r420_IntClass stack = new();
  function automatic bit watch_for_zero(r420_IntClass p);
    fork forever @p.a; join_none
    return (p.a == 0);
  endfunction
  bit y = watch_for_zero(stack);
endmodule

// catalog row 421 | 13.5 | COMP
// If a subroutine formal argument is declared output or inout, the
// corresponding expression in the call shall be restricted to an expression
// that is valid on the left-hand side of a procedural assignment.
module r421_m;
  task t(output int o);
  endtask
  initial t(1 + 2);
endmodule

// catalog row 432 | 13.5.3 | COMP
// If an unspecified (empty) argument, or an argument omitted via name binding,
// is used for a formal that has no default value, a compiler error shall be
// issued.
module r432_m;
  task read(int j = 0, int k, int data = 1);
  endtask
  initial begin
    read();
    read(1, , 7);
  end
endmodule
