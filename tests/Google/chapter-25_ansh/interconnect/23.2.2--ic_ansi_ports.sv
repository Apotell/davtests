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
:name: ic_ansi_ports
:description: ANSI interconnect port declarations in different styles
:tags: 23.2.2
*/

// Checks in this file:
//   CHK1: input, output and inout interconnect ports, one per line
//   CHK2: several ports in one declaration: input interconnect a, b
//   CHK3: subsequent ports inherit interconnect from the previous port
//   CHK4: packed interconnect port: input interconnect [3:0] v
//   CHK5: unpacked interconnect port: input interconnect u [0:1]

// CHK1
module m1(
  input  interconnect a,
  output interconnect y,
  inout  interconnect z
);
  leaf_in  l_a(a);
  leaf_out l_y(y);
  leaf_io  l_z(z);
endmodule

// CHK2 + CHK3
module m2(input interconnect a, b, c);
  leaf_in l_a(a);
  leaf_in l_b(b);
  leaf_in l_c(c);
endmodule

// CHK4 + CHK5
module m3(input interconnect [3:0] v, input interconnect u [0:1]);
  leaf_in4 l_v(v);
  leaf_in  l_u0(u[0]);
  leaf_in  l_u1(u[1]);
endmodule

module leaf_in(input wire p);
endmodule

module leaf_in4(input wire [3:0] p);
endmodule

module leaf_out(output wire p);
  assign p = 1'b0;
endmodule

module leaf_io(inout wire p);
endmodule

module top;
  wire a, y, z, b, c;
  wire [3:0] v;
  wire u [0:1];
  m1 u1(.a(a), .y(y), .z(z));
  m2 u2(a, b, c);
  m3 u3(.v(v), .u(u));
endmodule
