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
:name: mp_virtual_interface
:description: virtual interfaces that select a modport
:tags: 25.9.2
*/

// Checks in this file:
//   CHK1: virtual interface declaration with a modport: virtual A_Bus.TB
//   CHK2: interface instance (no modport) assigned to a virtual with modport
//   CHK3: virtual with no modport assigned to a virtual with modport
//   CHK4: clocking signal driven through a virtual clocking modport vif.sb.req
//   CHK5: virtual interface with modport stored in a class

interface A_Bus(input logic clk);
  wire req, gnt;
  clocking sb @(posedge clk);
    input gnt;
    output req;
  endclocking
  modport STB (clocking sb);
  modport TB (input gnt, output req);
endinterface

class Driver;
  // CHK5
  virtual A_Bus.STB vif;
  function new(virtual A_Bus.STB v);
    vif = v;
  endfunction
  task run();
    // CHK4
    vif.sb.req <= 1;
  endtask
endclass

module top;
  logic clk;
  A_Bus b1(clk);
  // CHK1
  virtual A_Bus.TB vtb;
  virtual A_Bus vplain;
  virtual A_Bus.STB vstb;
  initial begin
    Driver d;
    // CHK2
    vtb = b1;
    // CHK3
    vplain = b1;
    vstb = vplain;
    d = new(b1);
    d.run();
  end
endmodule
