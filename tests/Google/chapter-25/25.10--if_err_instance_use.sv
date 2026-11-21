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
:name: if_err_instance_use
:description: illegal uses of interface instances and their members
:should_fail_because: interface instances need parentheses, valid indices and members, are not values, and variables allow one continuous driver
:tags: 25.10
*/

// Errors in this file:
//   ERR1: interface instantiated without parentheses
//   ERR2: constant index outside an interface instance array
//   ERR3: reference to a member that the interface does not declare
//   ERR4: interface instance used as an expression value
//   ERR5: interface variable driven by two continuous assignments

interface bus_if;
  logic a;
endinterface

module m1;
  // ERR1
  bus_if u;
endmodule

module m2;
  bus_if arr[4]();
  // ERR2
  assign arr[4].a = 1'b0;
endmodule

module m3;
  bus_if u();
  logic y;
  // ERR3
  assign y = u.b;
endmodule

module m4;
  bus_if u();
  logic y;
  // ERR4
  assign y = u;
endmodule

module m5;
  bus_if u();
  // ERR5
  assign u.a = 1'b0;
  assign u.a = 1'b1;
endmodule

module top;
  m1 u1();
  m2 u2();
  m3 u3();
  m4 u4();
  m5 u5();
endmodule
