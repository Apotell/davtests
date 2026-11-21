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
:name: if_task_in_interface
:description: a task defined in an interface is called through an interface port
:tags: 25.7.1
*/

interface simple_bus (input logic clk);
  logic req, gnt;
  logic [7:0] addr, data;
  task initiatorRead(input logic [7:0] raddr);
    addr = raddr;
  endtask : initiatorRead
endinterface : simple_bus

module cpuMod (simple_bus b);
  logic [7:0] raddr;
  always @(posedge b.clk)
    b.initiatorRead(raddr);
endmodule

module top;
  logic clk = 0;
  simple_bus sb_intf(clk);
  cpuMod cpu(sb_intf);
endmodule
