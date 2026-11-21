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
:name: mp_unused_and_attributes
:description: modports that are declared but unused, attributes and names
:tags: 25.5
*/

// Checks in this file:
//   CHK1: interface with modports instantiated without using any modport
//   CHK2: modport containing a single signal
//   CHK3: attribute instance on a modport ports declaration
//   CHK4: modport name that is the same as a module name elsewhere
//   CHK5: typedef and localparam reached through a modport port although not listable (25.10)

module target;
endmodule

interface bus_if;
  typedef enum {Y, N} choice;
  localparam True = 1;
  choice Q;
  logic a, b;
  // CHK2
  modport only_a (input a);
  // CHK3
  modport attr ((* mark = 1 *) input a, (* mark = 2 *) output b);
  // CHK4
  modport target (input a, output b);
  modport mp (input Q);
endinterface

module user(bus_if b);
  assign b.a = b.b;
endmodule

// CHK5
module sub(interface.mp i);
  typedef i.choice yes_no;
  yes_no P;
  assign P = i.Q;
  initial top.u_if.Q = i.True ? i.Y : i.N;
endmodule

module top;
  // CHK1
  bus_if u_if();
  user u_user(u_if);
  sub u_sub(u_if.mp);
  target u_t();
endmodule
