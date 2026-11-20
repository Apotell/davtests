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
:name: if_inst_connections
:description: ways of connecting the ports of an interface instance
:tags: 25.3
*/

// Checks in this file:
//   CHK1: connection by position
//   CHK2: connection by name in a different order
//   CHK3: implicit .name connections
//   CHK4: .* wildcard connection
//   CHK5: several instances in one statement and in separate statements

interface bus_if(input logic clk, input logic rst);
  logic x;
endinterface

module top;
  logic clk, rst, c, r;
  // CHK1
  bus_if u1(clk, rst);
  // CHK2
  bus_if u2(.rst(r), .clk(c));
  // CHK3
  bus_if u3(.clk, .rst);
  // CHK4
  bus_if u4(.*);
  // CHK5
  bus_if a(clk, rst), b(clk, rst), c5(clk, rst);
  bus_if d(clk, rst);
  assign a.x = d.x;
endmodule
