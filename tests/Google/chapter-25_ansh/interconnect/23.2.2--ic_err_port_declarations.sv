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
:name: ic_err_port_declarations
:description: illegal interconnect port declarations and uses inside the module
:should_fail_because: interconnect ports cannot be signed, have defaults, or be used in assignments or procedural code
:tags: 23.2.2
*/

// Errors in this file:
//   ERR1: signed specified on an interconnect port
//   ERR2: default value specified on an interconnect input port
//   ERR3: interconnect output port assigned with a continuous assign
//   ERR4: interconnect input port read in an always_comb block
//   ERR5: interconnect input port printed with $display

// ERR1
module m1(input interconnect signed [3:0] a);
endmodule

// ERR2
module m2(input interconnect b = 1'b0);
endmodule

// ERR3
module m3(output interconnect y);
  assign y = 1'b1;
endmodule

// ERR4
module m4(input interconnect a);
  logic r;
  always_comb r = a;
endmodule

// ERR5
module m5(input interconnect a);
  initial $display("%b", a);
endmodule

module top;
  wire [3:0] w4;
  wire w;
  m1 u1(w4);
  m2 u2(w);
  m3 u3(w);
  m4 u4(w);
  m5 u5(w);
endmodule
