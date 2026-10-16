// Module ref ports (IEEE 1800-2023 Sec 23.2.2.3, 37.13, 37.14, 37.15).
// A ref port of a module is a vpiPort with vpiDirection == vpiRef and is also
// reached through the module's vpiIODecl, whose vpiExpr is a ref obj.

// ANSI ref port next to an ordinary input port.
module ansi_ref (
    ref   logic [7:0] r,
    input logic       a
);
  initial r[0] = a;
endmodule

// Non-ANSI ref port next to an ordinary input port.
module nonansi_ref (r, a);
  ref   logic [7:0] r;
  input logic       a;
  initial r[1] = a;
endmodule

module top;
  logic [7:0] x;
  logic       y;
  ansi_ref    u1 (.r(x), .a(y));
  nonansi_ref u2 (.r(x), .a(y));
endmodule
