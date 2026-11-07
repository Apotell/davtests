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
:name: if_decl_header_extras
:description: interface headers and bodies with imports, timeunits, attributes and extern
:tags: 25.3
*/

// Checks in this file:
//   CHK1: package import in the header before the port list
//   CHK2: package import inside the interface body
//   CHK3: timeunit and timeprecision inside the interface
//   CHK4: attribute instance on the interface declaration
//   CHK5: extern interface header followed by interface declared with (.*)

package bus_pkg;
  typedef logic [7:0] byte_t;
endpackage

// CHK1
interface c1_if import bus_pkg::*; (input logic clk);
  byte_t data;
endinterface

// CHK2
interface c2_if;
  import bus_pkg::byte_t;
  byte_t data;
endinterface

// CHK3
interface c3_if;
  timeunit 1ns;
  timeprecision 1ps;
  logic a;
endinterface

// CHK4
(* keep = 1 *)
interface c4_if;
  logic a;
endinterface

// CHK5
extern interface c5_if(input logic clk, output logic q);

interface c5_if(.*);
  logic r;
endinterface

module top;
  logic clk, q;
  c1_if u1(clk);
  c2_if u2();
  c3_if u3();
  c4_if u4();
  c5_if u5(.clk(clk), .q(q));
endmodule
