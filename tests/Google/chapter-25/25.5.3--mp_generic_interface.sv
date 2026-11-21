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
:name: mp_generic_interface
:description: generic interface ports combined with modports
:tags: 25.5.3
*/

// Checks in this file:
//   CHK1: generic 'interface a' port receiving sb_intf.target
//   CHK2: generic port receiving the other modport of the same instance
//   CHK3: generic port receiving a modport of a different interface type
//   CHK4: generic port with modport in header: interface.target t
//   CHK5: generic port receiving the full interface (no modport)

interface simple_bus(input logic clk);
  logic req, gnt;
  modport target (input req, clk, output gnt);
  modport initiator (input gnt, clk, output req);
endinterface

interface other_bus(input logic clk);
  logic req, gnt;
  modport target (input req, clk, output gnt);
endinterface

module memMod(interface a);
  always @(posedge a.clk) a.gnt <= a.req;
endmodule

module cpuMod(interface b);
endmodule

module hdrMod(interface.target t);
  always @(posedge t.clk) t.gnt <= t.req;
endmodule

module top;
  logic clk = 0;
  simple_bus sb_intf(clk);
  other_bus  ob_intf(clk);
  // CHK1
  memMod mem(sb_intf.target);
  // CHK2
  cpuMod cpu(sb_intf.initiator);
  // CHK3
  memMod mem2(ob_intf.target);
  // CHK4
  hdrMod hdr(.t(ob_intf));
  // CHK5
  cpuMod cpu2(sb_intf);
endmodule
