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
:name: mp_clocking_blocks
:description: clocking blocks referenced from modports (LRM A_Bus example)
:tags: 25.5.5
*/

// Checks in this file:
//   CHK1: modport containing only a clocking block (STB)
//   CHK2: asynchronous modports without the clocking block (DUT, TB)
//   CHK3: program with two clocking-modport ports driving b.sb.req
//   CHK4: wait on a clocking block input through the modport
//   CHK5: assert property declared inside the clocking block via b1.sb.p1

interface A_Bus(input logic clk);
  wire req, gnt;
  wire [7:0] addr, data;

  clocking sb @(posedge clk);
    input gnt;
    output req, addr;
    inout data;
    property p1; req ##[1:3] gnt; endproperty
  endclocking

  // CHK2
  modport DUT (input clk, req, addr, output gnt, inout data);
  // CHK1
  modport STB (clocking sb);
  // CHK2
  modport TB (input gnt, output req, addr, inout data);
endinterface

module dev1(A_Bus.DUT b);
endmodule

module dev2(A_Bus.DUT b);
endmodule

// CHK3
program T(A_Bus.STB b1, A_Bus.STB b2);
  // CHK5
  assert property (b1.sb.p1);
  initial begin
    b1.sb.req <= 1;
    // CHK4
    wait (b1.sb.gnt == 1);
    b1.sb.req <= 0;
    b2.sb.req <= 1;
    wait (b2.sb.gnt == 1);
    b2.sb.req <= 0;
  end
endprogram

module top;
  logic clk;
  A_Bus b1(clk);
  A_Bus b2(clk);
  dev1 d1(b1);
  dev2 d2(b2);
  T tb(b1, b2);
endmodule
