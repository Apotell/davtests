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
:name: if_member_access
:description: access to interface members
:tags: 25.10
*/

// Checks in this file:
//   CHK1: read and write members through the instance name
//   CHK2: full hierarchical path from another module top.u_if.a
//   CHK3: bit select and part select of a member
//   CHK4: struct field of a member

interface bus_if;
  typedef struct packed { logic [3:0] lo, hi; } pair_t;
  logic [3:0] a, b;
  logic [7:0] data;
  logic h;
  pair_t p;
endinterface

// CHK2
module sub;
  initial top.u_if.h = 1'b1;
endmodule

module top;
  bus_if u_if();
  sub u_sub();
  // CHK1
  initial begin
    u_if.a = 4'd3;
    u_if.b = u_if.a + 1;
  end
  // CHK3
  logic bit0;
  logic [3:0] nib;
  assign bit0 = u_if.data[0];
  assign nib = u_if.data[7:4];
  // CHK4
  logic [3:0] x;
  assign x = u_if.p.hi;
endmodule
