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
:name: if_modport_import_task
:description: modports import interface tasks by name
:tags: 25.7.2
*/

interface simple_bus (input logic clk);
  logic req, gnt;
  modport target (input req, clk, output gnt,
                  import targetRead, targetWrite);
  modport initiator (input gnt, clk, output req,
                     import initiatorRead);
  task initiatorRead(input logic [7:0] raddr);
  endtask
  task targetRead;
  endtask
  task targetWrite;
  endtask
endinterface : simple_bus

module memMod (simple_bus.target a);
  always @(posedge a.clk)
    if (a.req) a.targetRead;
    else a.targetWrite;
endmodule

module top;
  logic clk = 0;
  simple_bus sb_intf(clk);
  memMod mem(sb_intf);
endmodule
