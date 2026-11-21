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
:name: if_item_procedural
:description: procedural and behavioral items inside an interface
:tags: 25.3
*/

// Checks in this file:
//   CHK1: continuous assignment inside the interface
//   CHK2: always_ff and always_comb inside the interface
//   CHK3: initial and final procedures inside the interface
//   CHK4: enum typedef and initialized enum variable
//   CHK5: program declaration inside the interface

// CHK1
interface c1_if;
  logic a, b, y;
  assign y = a ^ b;
endinterface

// CHK2
interface c2_if(input logic clk);
  logic d, q, n;
  always_ff @(posedge clk) q <= d;
  always_comb n = ~q;
endinterface

// CHK3
interface c3_if;
  logic a;
  initial a = 1'b0;
  final $display("a=%b", a);
endinterface

// CHK4
interface c4_if;
  typedef enum logic [1:0] {IDLE, BUSY, DONE} state_t;
  state_t state = IDLE;
endinterface

// CHK5
interface c5_if;
  logic a;
  program p;
    initial a = 1'b1;
  endprogram
endinterface

module top;
  logic clk;
  c1_if u1();
  c2_if u2(clk);
  c3_if u3();
  c4_if u4();
  c5_if u5();
endmodule
