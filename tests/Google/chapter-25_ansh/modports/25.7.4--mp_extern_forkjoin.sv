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
:name: mp_extern_forkjoin
:description: same task exported by multiple modules using extern forkjoin
:tags: 25.7.4
*/

// Checks in this file:
//   CHK1: extern forkjoin task declared in the interface
//   CHK2: extern (non forkjoin) task declared in the interface
//   CHK3: two instances of the same module exporting countTargets
//   CHK4: task exported through the modport and defined as a.countTargets
//   CHK5: initiator calls the forkjoin task through its modport

interface simple_bus(input logic clk);
  logic req;
  // CHK1
  extern forkjoin task countTargets();
  // CHK2
  extern task Ping(input int v);

  modport target (input req, clk, export countTargets);
  modport pinger (input clk, export Ping);
  modport initiator (input clk, output req, import countTargets);
endinterface

module memMod(interface a);
  int hits;
  // CHK4
  task a.countTargets();
    hits++;
  endtask
endmodule

module pingMod(interface a);
  task a.Ping(input int v);
  endtask
endmodule

module cpuMod(interface b);
  // CHK5
  initial b.countTargets();
endmodule

module top;
  logic clk = 0;
  simple_bus sb_intf(clk);
  // CHK3
  memMod mem1(sb_intf.target);
  memMod mem2(sb_intf.target);
  pingMod png(sb_intf.pinger);
  cpuMod cpu(sb_intf.initiator);
endmodule
