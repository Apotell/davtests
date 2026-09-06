/*
:name: chapter13_error_rules
:description: IEEE 1800-2023 Clause 13 (Tasks and functions) error scenarios
:tags: 13.4.1 13.4.3 13.5.3
*/

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
