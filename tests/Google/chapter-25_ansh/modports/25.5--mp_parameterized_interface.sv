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
:name: mp_parameterized_interface
:description: modports in a parameterized interface
:tags: 25.5
*/

// Checks in this file:
//   CHK1: modport on signals whose width depends on a parameter
//   CHK2: module header uses a modport of a parameterized interface
//   CHK3: instance with overridden parameter connected through modport
//   CHK4: module reads the parameter through the modport port b.W
//   CHK5: two instances with different parameters using the same modport module

interface bus_if #(parameter int W = 8);
  logic [W-1:0] data, result;
  // CHK1
  modport target (input data, output result);
endinterface

// CHK2 + CHK4
module proc(bus_if.target b);
  logic [b.W-1:0] tmp;
  assign tmp = b.data;
  assign b.result = ~tmp;
endmodule

module top;
  // CHK3
  bus_if #(16) b16();
  bus_if #(.W(4)) b4();
  // CHK5
  proc u1(.b(b16));
  proc u2(.b(b4));
endmodule
