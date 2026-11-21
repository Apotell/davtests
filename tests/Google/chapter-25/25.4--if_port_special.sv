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
:name: if_port_special
:description: ref ports, default values, non-ANSI ports and shared port nets
:tags: 25.4
*/

// Checks in this file:
//   CHK1: ref port on an interface
//   CHK2: input port with a default value, left unconnected
//   CHK3: non-ANSI port list with directions in the body
//   CHK4: non-ANSI directions declared together: input a, b;
//   CHK5: one clock net shared by two interface instances (LRM simple_bus)

// CHK1
interface c1_if(ref int counter);
  initial counter = 5;
endinterface

// CHK2
interface c2_if(input logic clk, input logic en = 1'b1);
  logic q;
  always_ff @(posedge clk) if (en) q <= ~q;
endinterface

// CHK3
interface c3_if(clk, a, y);
  input clk;
  input [7:0] a;
  output [7:0] y;
  logic [7:0] y;
  assign y = a;
endinterface

// CHK4
interface c4_if(a, b, y);
  input a, b;
  output y;
  assign y = a & b;
endinterface

// CHK5
interface simple_bus(input logic clk);
  logic req, gnt;
endinterface

module top;
  int counter;
  logic clk = 0;
  logic [7:0] a8, y8;
  logic a, b, y;
  c1_if u1(counter);
  c2_if u2(.clk(clk));
  c3_if u3(clk, a8, y8);
  c4_if u4(a, b, y);
  simple_bus sb_intf1(clk);
  simple_bus sb_intf2(clk);
endmodule
