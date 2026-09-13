/*
:name: chapter13_error_rules
:description: IEEE 1800-2023 Clause 13 error scenarios
:tags: 13.4 13.4.1 13.4.4 13.5 13.5.3
*/

// Every scenario below is derived from one row of the SV error catalog
// (docs/error_catalog.xml). The "catalog row N" comment is the link back to
// that row; the matching gtest is named RowN_...

// catalog row 402 | 13.4 | COMP
// It shall be illegal to call a function with output, inout or (non-const)
// ref arguments in an event expression, in an expression within a
// procedural continuous assignment, or in an expression that is not within
// a procedural statement.
module r402_m;
  int a, b;
  wire w;
  function int f(output int o);
    o = 1;
    return 2;
  endfunction
  assign w = f(a);       // ILLEGAL: not within a procedural statement
  always @(f(a)) ;       // ILLEGAL: in an event expression
endmodule

// catalog row 404 | 13.4.1 | COMP
// Calling a nonvoid function as if it had no return value shall be legal
// but shall issue a warning; casting the call to void suppresses the
// warning.
module r404_m;
  function int f();
    return 1;
  endfunction
  initial begin
    f();            // legal, but a warning shall be issued
    void'(f());     // no warning
  end
endmodule

// catalog row 420 | 13.4.4 | LINT
// Calling a function that schedules an event that cannot become active
// until after the function returns is allowed only when the calling thread
// was created by an initial procedure, an always procedure, or a fork from
// one of those, and in a context where a side effect is allowed;
// implementations shall issue an error at compile time or run time
// otherwise.
class r420_IntClass; int a; endclass
module r420_m;
  r420_IntClass stack = new();
  function automatic bit watch_for_zero(r420_IntClass p);
    fork forever @p.a; join_none
    return (p.a == 0);
  endfunction
  bit y = watch_for_zero(stack); // ILLEGAL: variable initializer is not an initial/always/fork thread
endmodule

// catalog row 421 | 13.5 | COMP
// If a subroutine formal argument is declared output or inout, the
// corresponding expression in the call shall be restricted to an
// expression that is valid on the left-hand side of a procedural
// assignment.
module r421_m;
  task t(output int o);
  endtask
  initial t(1 + 2); // ILLEGAL: output argument bound to a non-lvalue expression
endmodule

// catalog row 432 | 13.5.3 | COMP
// If an unspecified (empty) argument, or an argument omitted via name
// binding, is used for a formal that has no default value, a compiler
// error shall be issued.
module r432_m;
  task read(int j = 0, int k, int data = 1);
  endtask
  initial begin
    read();          // ILLEGAL: k has no default value
    read(1, , 7);    // ILLEGAL: k has no default value
  end
endmodule
