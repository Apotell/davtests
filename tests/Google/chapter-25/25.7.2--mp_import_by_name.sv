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
:name: mp_import_by_name
:description: tasks and functions imported into modports by name
:tags: 25.7.2
*/

// Checks in this file:
//   CHK1: several tasks imported in one import list
//   CHK2: separate import groups in one modport
//   CHK3: function imported and called in a continuous assign
//   CHK4: task with no arguments called without parentheses
//   CHK5: module without modport sees all tasks (omniMod)

interface simple_bus(input logic clk);
  logic req, gnt;
  logic [7:0] addr, data;
  logic [1:0] mode;
  logic start;

  // CHK1
  modport target (input req, addr, mode, start, clk, output gnt, ref data,
                  import targetRead, targetWrite);
  // CHK2 + CHK3
  modport initiator (input gnt, clk, output req, addr, mode, start, ref data,
                     import initiatorRead, import initiatorWrite,
                     import parity);

  task initiatorRead(input logic [7:0] raddr);
  endtask
  task targetRead;
  endtask
  task initiatorWrite(input logic [7:0] waddr);
  endtask
  task targetWrite;
  endtask
  function automatic logic parity(logic [7:0] v);
    return ^v;
  endfunction
endinterface : simple_bus

module memMod(interface a);
  always @(a.start)
    // CHK4
    if (a.mode[0] == 1'b0) a.targetRead;
    else a.targetWrite;
endmodule

module cpuMod(interface b);
  logic [7:0] raddr;
  logic p;
  // CHK3
  assign p = b.parity(raddr);
  always @(posedge b.clk) b.initiatorRead(raddr);
endmodule

// CHK5
module omniMod(interface b);
  initial begin
    b.targetRead;
    b.initiatorWrite(8'h00);
  end
endmodule

module top;
  logic clk = 0;
  simple_bus sb_intf(clk);
  memMod mem(sb_intf.target);
  cpuMod cpu(sb_intf.initiator);
  omniMod omni(sb_intf);
endmodule
