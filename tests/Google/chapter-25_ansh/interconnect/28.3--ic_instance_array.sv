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
:name: ic_instance_array
:description: interconnect expressions connected to arrays of instances
:tags: 28.3
*/

// Checks in this file:
//   CHK1: interconnect [3:0] to four instances with 1-bit ports (length equals array length)
//   CHK2: interconnect [0:7] to an instance array declared [0:7]
//   CHK3: element of an unpacked interconnect array to an instance array
//   CHK4: LRM aBus[0:3][0:1] connected to driver and cmp instance arrays
//   CHK5: interconnect port of a module forwarded to an instance array

module bit_leaf(inout wire p);
endmodule

module driver(output wire out[0:1]);
endmodule

module cmp(input wire inA[0:1], input logic rst, output logic equal);
endmodule

module fwd(inout interconnect [3:0] q);
  // CHK5
  bit_leaf u[3:0](q);
endmodule

module top;
  // CHK1
  interconnect [3:0] n4;
  bit_leaf a4[3:0](n4);
  // CHK2
  interconnect [0:7] n8;
  bit_leaf a8[0:7](n8);
  // CHK3
  interconnect [1:0] arr [0:2];
  bit_leaf a2[1:0](arr[1]);
  // CHK4
  interconnect aBus[0:3][0:1];
  logic [0:3] dBus;
  logic rst;
  driver driverArray[0:3](aBus);
  cmp    cmpArray[0:3](aBus, rst, dBus);
  // CHK5
  interconnect [3:0] f;
  fwd u_fwd(f);
endmodule
