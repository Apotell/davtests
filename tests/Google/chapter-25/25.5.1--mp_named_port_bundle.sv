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
:name: mp_named_port_bundle
:description: LRM 25.5.1 named port bundle example with extra checks
:tags: 25.5.1
*/

// Checks in this file:
//   CHK1: modport with input, output and ref groups
//   CHK2: interface port clk listed inside a modport
//   CHK3: modport selected in the module header (simple_bus.target)
//   CHK4: non-blocking write to an output modport signal
//   CHK5: ref modport signal written by one module and read by another

interface simple_bus(input logic clk);
  logic req, gnt;
  logic [7:0] addr, data;
  logic [1:0] mode;
  logic start, rdy;
  // CHK1 + CHK2
  modport target (input req, addr, mode, start, clk,
                  output gnt, rdy,
                  ref data);
  modport initiator (input gnt, rdy, clk,
                     output req, addr, mode, start,
                     ref data);
endinterface : simple_bus

// CHK3
module memMod(simple_bus.target a);
  logic avail;
  // CHK4
  always @(posedge a.clk) a.gnt <= a.req & avail;
  // CHK5
  always @(posedge a.clk) a.data <= a.addr;
endmodule

module cpuMod(simple_bus.initiator b);
  logic [7:0] seen;
  // CHK5
  always @(posedge b.clk) seen <= b.data;
endmodule

module top;
  logic clk = 0;
  simple_bus sb_intf(clk);
  initial repeat (10) #10 clk++;
  memMod mem(.a(sb_intf));
  cpuMod cpu(.b(sb_intf));
endmodule
