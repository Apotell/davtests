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
:name: if_err_virtual
:description: illegal virtual interface assignments
:should_fail_because: virtual interface assignments require identical types and parameters and no outside defparam or hierarchical references
:tags: 25.9
*/

// Errors in this file:
//   ERR1: instance with a different parameter value assigned
//   ERR2: instance of a different interface type assigned
//   ERR3: instance changed by an outside defparam assigned
//   ERR4: interface containing an outside hierarchical reference used as virtual

interface p_if #(parameter int W = 8);
  logic [W-1:0] data;
endinterface

interface if_a;
  logic x;
endinterface

interface if_b;
  logic x;
endinterface

interface h_if;
  logic a;
  assign a = top.ext;
endinterface

module top;
  logic ext;
  // ERR1
  p_if #(16) u1();
  virtual p_if #(8) v1;
  initial v1 = u1;
  // ERR2
  if_b u2();
  virtual if_a v2;
  initial v2 = u2;
  // ERR3
  p_if u3();
  defparam u3.W = 8;
  virtual p_if v3;
  initial v3 = u3;
  // ERR4
  h_if u4();
  virtual h_if v4;
  initial v4 = u4;
endmodule
