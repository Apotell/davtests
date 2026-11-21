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
:name: if_modport_connect_bundle
:description: the modport is selected in an ordered port connection of the module instance
:tags: 25.5.2
*/

interface simple_bus (input logic clk);
  logic req, gnt;
  logic [7:0] addr, data;
  logic [1:0] mode;
  logic start, rdy;
  modport target (input req, addr, mode, start, clk,
                 output gnt, rdy,
                 ref data);
  modport initiator (input gnt, rdy, clk,
                  output req, addr, mode, start,
                  ref data);
endinterface : simple_bus

module memMod (simple_bus a);
  logic avail;
  always @(posedge a.clk)
    a.gnt <= a.req & avail;
endmodule

module cpuMod (simple_bus b);
endmodule

module top;
  logic clk = 0;
  simple_bus sb_intf(clk);
  initial repeat (10) #10 clk++;
  memMod mem(sb_intf.target);
  cpuMod cpu(sb_intf.initiator);
endmodule
