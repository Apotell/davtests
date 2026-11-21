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
:name: if_param_override
:description: parameterized interfaces and the ways to override their parameters
:tags: 25.8
*/

// Checks in this file:
//   CHK1: parameterized interface instantiated without override
//   CHK2: override by position #(16)
//   CHK3: override by name #(.W(16))
//   CHK4: several parameters in one list overridden by name in any order
//   CHK5: several parameters each with its own keyword overridden by position

interface w_if #(parameter int W = 8);
  logic [W-1:0] data;
endinterface

interface aw_dw_if #(parameter int AW = 4, DW = 8);
  logic [AW-1:0] addr;
  logic [DW-1:0] data;
endinterface

interface sep_if #(
  parameter int AW = 4,
  parameter int DW = 8
);
  logic [AW-1:0] addr;
  logic [DW-1:0] data;
endinterface

module top;
  // CHK1
  w_if u1();
  // CHK2
  w_if #(16) u2();
  logic [15:0] d;
  assign d = u2.data;
  // CHK3
  w_if #(.W(16)) u3();
  // CHK4
  aw_dw_if #(.DW(32), .AW(10)) u4();
  // CHK5
  sep_if #(10, 32) u5();
endmodule
