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
:name: if_port_ansi_styles
:description: ANSI interface port declaration styles
:tags: 25.4
*/

// Checks in this file:
//   CHK1: one port per line: input a, / input b, / output y
//   CHK2: ports sharing one declaration: input logic a, b
//   CHK3: later ports inherit direction and type: input logic [3:0] a, b, c
//   CHK4: input, output and inout in one header
//   CHK5: explicit wire and var port kinds

// CHK1
interface c1_if(
  input  logic a,
  input  logic b,
  output logic y
);
  assign y = a & b;
endinterface

// CHK2
interface c2_if(input logic a, b, output logic y);
  assign y = a | b;
endinterface

// CHK3
interface c3_if(input logic [3:0] a, b, c, output logic [3:0] y);
  assign y = a + b + c;
endinterface

// CHK4
interface c4_if(input logic clk, output logic q, inout wire d);
  always_ff @(posedge clk) q <= d;
endinterface

// CHK5
interface c5_if(input wire logic a, input var logic b, output var logic y);
  assign y = a ^ b;
endinterface

module top;
  logic a, b, y1, y2, y5, clk, q;
  logic [3:0] a4, b4, c4, y4;
  wire d;
  c1_if u1(.a(a), .b(b), .y(y1));
  c2_if u2(a, b, y2);
  c3_if u3(a4, b4, c4, y4);
  c4_if u4(clk, q, d);
  c5_if u5(a, b, y5);
endmodule
