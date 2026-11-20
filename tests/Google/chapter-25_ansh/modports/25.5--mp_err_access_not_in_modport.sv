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
:name: mp_err_access_not_in_modport
:description: accessing interface items that the selected modport does not list
:should_fail_because: only items listed in the selected modport are accessible through it
:tags: 25.5
*/

// Errors in this file:
//   ERR1: reading a signal that exists in the interface but not in the modport
//   ERR2: writing a signal that is not in the modport
//   ERR3: calling an interface task that the modport does not import
//   ERR4: referencing a clocking block not listed in the modport
//   ERR5: modport selected at the instance hides a signal used by the module

interface bus_if(input logic clk);
  logic a, hidden;
  clocking cb @(posedge clk);
    input a;
  endclocking
  task secret();
  endtask
  modport mp (input a);
endinterface

// ERR1
module m1(bus_if.mp p);
  logic y;
  assign y = p.hidden;
endmodule

// ERR2
module m2(bus_if.mp p);
  assign p.hidden = 1'b1;
endmodule

// ERR3
module m3(bus_if.mp p);
  initial p.secret();
endmodule

// ERR4
module m4(bus_if.mp p);
  initial @(p.cb) $display("tick");
endmodule

// ERR5
module m5(bus_if p);
  logic y;
  assign y = p.hidden;
endmodule

module top;
  logic clk;
  bus_if b(clk);
  m1 u1(b);
  m2 u2(b);
  m3 u3(b);
  m4 u4(b);
  m5 u5(b.mp);
endmodule
