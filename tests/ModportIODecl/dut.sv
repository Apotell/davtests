// Modports as named groups of io decls (IEEE 1800-2023 Sec 25.5, 25.10, 37.7, 37.13).
// Examples taken from Sec 25.5.1, 25.5.4, 25.5.5 and 25.10.

// 25.5.1 named port bundle
interface simple_bus (input logic clk);
  logic req, gnt;
  logic [7:0] addr, data;
  logic [1:0] mode;
  logic start, rdy;
  modport target (input req, addr, mode, start, clk, output gnt, rdy, ref data);
  modport initiator (input gnt, rdy, clk, output req, addr, mode, start, ref data);
endinterface

module memMod (simple_bus.target a);
  logic avail;
  always @(posedge a.clk) a.gnt <= a.req & avail;
endmodule

// 25.5.4 modport expressions
interface I;
  logic [7:0] r;
  const int x = 1;
  bit R;
  modport A (output .P(r[3:0]), input .Q(x), R);
  modport B (output .P(r[7:4]), input .Q(2), R);
endinterface

module M (I.A i);
  initial i.P = i.Q;
endmodule

// 25.5.5 clocking blocks and modports
interface A_Bus (input logic clk);
  wire req, gnt;
  wire [7:0] addr, data;
  clocking sb @(posedge clk);
    input gnt;
    output req, addr;
    inout data;
  endclocking
  modport DUT (input clk, req, addr, output gnt, inout data);
  modport STB (clocking sb);
endinterface

module stb (A_Bus.STB b);
  initial b.sb.req <= 1'b1;
endmodule

// 25.10 access to interface objects
interface ebus_i;
  integer I;
  typedef enum {Y, N} choice;
  choice Q;
  localparam True = 1;
  modport mp (input Q);
endinterface

module sub (ebus_i.mp i);
  typedef i.choice yes_no;
  yes_no P;
  assign P = i.Q;
  initial i.I = 0;  // illegal: I is not in modport mp
endmodule

// 23.2.2.4: only an input port may have a default value; an interface port may not.
module bad_default (simple_bus x = 1'b0);
endmodule
