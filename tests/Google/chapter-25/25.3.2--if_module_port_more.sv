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
:name: if_module_port_more
:description: interface ports mixed with signals, non-ANSI, hierarchy and interfaces with interface ports
:tags: 25.3.2
*/

// Checks in this file:
//   CHK1: interface port mixed with ordinary input/output ports
//   CHK2: interface port declared in a non-ANSI port list
//   CHK3: interface passed from top through a middle module to a leaf
//   CHK4: interface whose port is another interface

interface bus_if;
  logic x;
endinterface

// CHK1
module c1_sub(input logic clk, bus_if b, output logic q);
  always_ff @(posedge clk) q <= b.x;
endmodule

// CHK2
module c2_sub(b);
  bus_if b;
endmodule

// CHK3
module c3_leaf(bus_if b);
  assign b.x = 1'b1;
endmodule

module c3_mid(bus_if b);
  c3_leaf u_leaf(.b(b));
endmodule

// CHK4
interface c4_outer_if(bus_if i);
  logic y;
  assign y = i.x;
endinterface

module top;
  logic clk, q;
  bus_if b1(), b2(), b3(), b4();
  c1_sub u1(.clk(clk), .b(b1), .q(q));
  c2_sub u2(b2);
  c3_mid u3(.b(b3));
  c4_outer_if u4(b4);
endmodule
