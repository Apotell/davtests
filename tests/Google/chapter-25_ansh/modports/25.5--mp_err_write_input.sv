/*
 Copyright 2020 Apotell

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

/*
:name: mp_err_write_input
:description: writing to signals that the modport declares as input
:should_fail_because: a modport input variable shall not be written from the module
:tags: 25.5
*/

// Errors in this file:
//   ERR1: continuous assign to an input modport variable
//   ERR2: always_comb assignment to an input modport variable
//   ERR3: always_ff non-blocking assignment to an input modport variable
//   ERR4: blocking assignment in initial to an input modport variable
//   ERR5: increment operator on an input modport variable

interface bus_if(input logic clk);
  logic a, b;
  int   cnt;
  modport rd (input clk, a, b, cnt);
endinterface

// ERR1
module m1(bus_if.rd p);
  assign p.a = 1'b0;
endmodule

// ERR2
module m2(bus_if.rd p);
  always_comb p.b = 1'b1;
endmodule

// ERR3
module m3(bus_if.rd p);
  always_ff @(posedge p.clk) p.a <= ~p.a;
endmodule

// ERR4
module m4(bus_if.rd p);
  initial p.b = 1'b0;
endmodule

// ERR5
module m5(bus_if.rd p);
  initial p.cnt++;
endmodule

module top;
  logic clk;
  bus_if b(clk);
  m1 u1(b);
  m2 u2(b);
  m3 u3(b);
  m4 u4(b);
  m5 u5(b);
endmodule
