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
:name: mp_import_prototype_export
:description: import with full prototypes and export of tasks defined in a module
:tags: 25.7.3
*/

// Checks in this file:
//   CHK1: export Read, Write by name in the target modport
//   CHK2: import task prototypes in the initiator modport
//   CHK3: tasks defined in the module with hierarchical names task a.Read
//   CHK4: import function prototype with a default argument value
//   CHK5: call using the default argument from the prototype

interface simple_bus(input logic clk);
  logic req, gnt;
  logic [7:0] addr, data;

  // CHK1
  modport target (input req, addr, clk, output gnt, ref data,
                  export Read, Write, export calc);
  // CHK2 + CHK4
  modport initiator (input gnt, clk, output req, addr, ref data,
                     import task Read(input logic [7:0] raddr),
                            task Write(input logic [7:0] waddr),
                     import function int calc(int v = 3));
endinterface : simple_bus

module memMod(interface a);
  logic avail;
  // CHK3
  task a.Read(input logic [7:0] raddr);
    avail = 0;
    avail = 1;
  endtask
  task a.Write(input logic [7:0] waddr);
    avail = 0;
    avail = 1;
  endtask
  function int a.calc(int v);
    return v * 2;
  endfunction
endmodule

module cpuMod(interface b);
  logic [7:0] raddr;
  int r;
  always @(posedge b.clk) begin
    b.Read(raddr);
    b.Write(raddr);
    // CHK5
    r = b.calc();
  end
endmodule

module top;
  logic clk = 0;
  simple_bus sb_intf(clk);
  memMod mem(sb_intf.target);
  cpuMod cpu(sb_intf.initiator);
endmodule
