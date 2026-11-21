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
:name: if_module_port_generic_arrays
:description: generic interface ports and interface arrays on module ports
:tags: 25.3.3
*/

// Checks in this file:
//   CHK1: generic 'interface a' port connected to a concrete instance
//   CHK2: one generic-port module instantiated with two different interface types
//   CHK3: one element of an interface array connected to a module port
//   CHK4: module port declared as an array of interfaces
//   CHK5: array of module instances each getting one element of an interface array

interface if_a;
  logic x;
endinterface

interface if_b;
  logic x;
  logic extra;
endinterface

// CHK1 + CHK2
module gen_sub(interface i);
  logic y;
  assign y = i.x;
endmodule

// CHK3
module one(if_a b);
endmodule

// CHK4
module pair(if_a b[2]);
  assign b[0].x = b[1].x;
endmodule

// CHK5
module each(if_a b);
  assign b.x = 1'b0;
endmodule

module top;
  if_a ia();
  if_b ib();
  if_a arr3[3]();
  if_a arr2[2]();
  if_a arr4[4]();
  gen_sub u1(.i(ia));
  gen_sub u2(.i(ib));
  one u3(.b(arr3[1]));
  pair u4(.b(arr2));
  each u5[4](.b(arr4));
endmodule
