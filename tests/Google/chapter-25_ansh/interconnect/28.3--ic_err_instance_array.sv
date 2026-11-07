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
:name: ic_err_instance_array
:description: interconnect expressions whose bit length differs from the instance array length
:should_fail_because: an interconnect port expression on an instance array must have bit length equal to the array length
:tags: 28.3
*/

// Errors in this file:
//   ERR1: interconnect [7:0] connected to four 2-bit instances (no part-select split for interconnect)
//   ERR2: interconnect [2:0] connected to an array of four instances
//   ERR3: interconnect [5:0] connected to an array of two instances
//   ERR4: interconnect port of width 8 forwarded to an array of four instances

module leaf1(inout wire p);
endmodule

module leaf2(inout wire [1:0] p);
endmodule

module leaf3(inout wire [2:0] p);
endmodule

module fwd(inout interconnect [7:0] q);
  // ERR4
  leaf2 u[3:0](q);
endmodule

module top;
  interconnect [7:0] n1;
  interconnect [2:0] n2;
  interconnect [5:0] n3;
  interconnect [7:0] n4;
  // ERR1
  leaf2 a1[3:0](n1);
  // ERR2
  leaf1 a2[3:0](n2);
  // ERR3
  leaf3 a3[1:0](n3);
  fwd u4(n4);
endmodule
