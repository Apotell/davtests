package pkg;
  typedef logic [3:0] nib_t;
  parameter type PT = int;
endpackage

module with_ports #(type T = int, localparam type LP = byte) ();
  parameter type BODY = logic;
  localparam type L = bit;
endmodule

module no_ports;
  import pkg::*;
  parameter type P = shortint;
  parameter type N = nib_t;
endmodule

class C #(type CT = real);
  parameter type CB = longint;
endclass
