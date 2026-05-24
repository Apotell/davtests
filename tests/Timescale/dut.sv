module  m1(input logic a);
  timeunit 10 ns/1 ps;

  module m11(input logic a);
    timeunit 10 ns/10 ps;
  endmodule

  module  m12(input logic a);
    timeprecision 1 ps;
  endmodule
endmodule

`timescale 1 ns/1 ps 

module m2(input logic a);
endmodule
