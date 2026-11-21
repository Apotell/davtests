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
:name: ic_wire_side_assignments
:description: legal code next to interconnect: assignments happen on the typed side only
:tags: 6.6.8
*/

// Checks in this file:
//   CHK1: module drives its wire output with assign; parent connects interconnect
//   CHK2: module reads its wire input in always_comb; parent connects interconnect
//   CHK3: $display of the typed port inside the child, not of the interconnect
//   CHK4: interconnect between a driver and two receivers
//   CHK5: interconnect delay #2 with typed endpoints

module src(output wire o);
  // CHK1
  assign o = 1'b1;
endmodule

module dst(input wire i);
  logic seen;
  // CHK2
  always_comb seen = i;
  // CHK3
  initial #1 $display("i=%b", i);
endmodule

module top;
  interconnect n;
  src u_s(n);
  // CHK4
  dst u_d1(n);
  dst u_d2(n);
  // CHK5
  interconnect #2 nd;
  src u_s2(nd);
  dst u_d3(nd);
endmodule
