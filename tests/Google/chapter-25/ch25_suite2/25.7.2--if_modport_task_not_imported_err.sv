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
:name: if_modport_task_not_imported_err
:description: a module calls an interface task that its modport does not import
:should_fail_because: a module connected through a modport only has access to the subroutines imported by that modport
:tags: 25.7.2
*/

interface simple_bus (input logic clk);
  modport target (input clk, import targetRead);
  task targetRead;
  endtask
  task initiatorRead;
  endtask
endinterface : simple_bus

module memMod (simple_bus.target a);
  always @(posedge a.clk)
    a.initiatorRead;
endmodule

module top;
  logic clk = 0;
  simple_bus sb_intf(clk);
  memMod mem(sb_intf);
endmodule
