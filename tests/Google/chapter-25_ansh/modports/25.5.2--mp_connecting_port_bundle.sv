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
:name: mp_connecting_port_bundle
:description: LRM 25.5.2 connecting port bundle example with extra checks
:tags: 25.5.2
*/

// Checks in this file:
//   CHK1: module header uses only the interface name
//   CHK2: modport given positionally at the instance (sb_intf.target)
//   CHK3: modport given by name at the instance (.b(sb_intf.initiator))
//   CHK4: output modport signal driven by always_comb
//   CHK5: input modport signal used as a clock in always_ff

interface simple_bus(input logic clk);
  logic req, gnt;
  logic [7:0] addr, data;
  logic start, rdy;
  modport target (input req, addr, start, clk, output gnt, rdy, ref data);
  modport initiator (input gnt, rdy, clk, output req, addr, start, ref data);
endinterface : simple_bus

// CHK1
module memMod(simple_bus a);
  // CHK4
  always_comb a.rdy = a.start;
  // CHK5
  always_ff @(posedge a.clk) a.gnt <= a.req;
endmodule

module cpuMod(simple_bus b);
  always_ff @(posedge b.clk) b.req <= ~b.gnt;
endmodule

module top;
  logic clk = 0;
  simple_bus sb_intf(clk);
  // CHK2
  memMod mem(sb_intf.target);
  // CHK3
  cpuMod cpu(.b(sb_intf.initiator));
endmodule
