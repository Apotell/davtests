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
:name: if_modport_clocking
:description: a modport lists a clocking block declared in the same interface
:tags: 25.5.5
*/

interface A_Bus (input logic clk);
  wire req, gnt;
  wire [7:0] addr, data;
  clocking sb @(posedge clk);
    input gnt;
    output req, addr;
    inout data;
  endclocking
  modport DUT (input clk, req, addr, output gnt, inout data);
  modport STB (clocking sb);
  modport TB (input gnt, output req, addr, inout data);
endinterface

module dev1 (A_Bus.DUT b);
endmodule

program T (A_Bus.STB b1);
endprogram

module top;
  logic clk;
  A_Bus b1 (clk);
  dev1 d1 (b1);
  T tb (b1);
endmodule
