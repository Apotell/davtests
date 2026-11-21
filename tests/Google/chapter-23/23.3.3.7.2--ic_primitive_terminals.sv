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
:name: ic_primitive_terminals
:description: interconnect nets connected to primitive and UDP terminals (treated as scalar wire)
:tags: 23.3.3.7.2
*/

// Checks in this file:
//   CHK1: interconnect on the output and inputs of an and gate
//   CHK2: interconnect between buf and not gates
//   CHK3: interconnect on a bufif1 enable terminal
//   CHK4: interconnect on a pullup terminal
//   CHK5: interconnect on the terminals of a user-defined primitive

primitive udp_and(output y, input a, input b);
  table
    0 ? : 0;
    ? 0 : 0;
    1 1 : 1;
  endtable
endprimitive

module top;
  interconnect a, b;
  interconnect y1, y2;
  interconnect y3, en;
  interconnect pu, uy;
  // CHK1
  and g_and(y1, a, b);
  // CHK2
  buf g_buf(y2, a);
  not g_not(y3, y2);
  // CHK3
  bufif1 g_bif(y1, b, en);
  // CHK4
  pullup g_pu(pu);
  // CHK5
  udp_and g_udp(uy, a, b);
endmodule
