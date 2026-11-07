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
:name: if_param_kinds
:description: type parameters, body parameters, defparam and parameter access through a port
:tags: 25.8
*/

// Checks in this file:
//   CHK1: type parameter overridden with int
//   CHK2: parameter declared in the body and overridden
//   CHK3: parameter changed with defparam
//   CHK4: module reads the parameter through its interface port b.W
//   CHK5: one override applied to several instances and an array (LRM #(100) example)

// CHK1
interface c1_if #(parameter type T = logic [7:0]);
  T data;
endinterface

// CHK2
interface c2_if;
  parameter int W = 4;
  logic [W-1:0] data;
endinterface

interface w_if #(parameter int W = 8);
  logic [W-1:0] data;
endinterface

// CHK4
module c4_sub(w_if b);
  logic [b.W-1:0] copy;
  assign copy = b.data;
endmodule

module top;
  c1_if #(.T(int)) u1();
  int x;
  assign x = u1.data;
  c2_if #(.W(12)) u2();
  // CHK3
  w_if u3();
  defparam u3.W = 4;
  w_if #(12) b4();
  c4_sub u4(.b(b4));
  // CHK5
  w_if #(100) scalar1(), vector[9:0]();
endmodule
