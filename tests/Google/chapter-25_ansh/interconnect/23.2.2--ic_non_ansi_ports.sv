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
:name: ic_non_ansi_ports
:description: non-ANSI interconnect port declarations
:tags: 23.2.2
*/

// Checks in this file:
//   CHK1: input interconnect a; declared in the module body
//   CHK2: output interconnect [3:0] b; packed non-ANSI port
//   CHK3: inout interconnect c; non-ANSI port
//   CHK4: two ports declared together: input interconnect d, e;
//   CHK5: non-ANSI interconnect ports connected by position and by name

module m(a, b, c, d, e);
  // CHK1
  input interconnect a;
  // CHK2
  output interconnect [3:0] b;
  // CHK3
  inout interconnect c;
  // CHK4
  input interconnect d, e;

  leaf1 l_a(a);
  leaf4 l_b(b);
  leaf1 l_c(c);
  leaf1 l_d(d);
  leaf1 l_e(e);
endmodule

module leaf1(inout wire p);
endmodule

module leaf4(inout wire [3:0] p);
endmodule

module top;
  wire a, c, d, e;
  wire [3:0] b;
  // CHK5
  m u_pos(a, b, c, d, e);
  m u_named(.e(e), .d(d), .c(c), .b(b), .a(a));
endmodule
