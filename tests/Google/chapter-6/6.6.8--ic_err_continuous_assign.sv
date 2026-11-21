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
:name: ic_err_continuous_assign
:description: interconnect nets used in continuous or procedural continuous assignments
:should_fail_because: an interconnect net shall not be used in any continuous or procedural continuous assignment
:tags: 6.6.8
*/

// Errors in this file:
//   ERR1: continuous assign to an interconnect net: assign w1 = 1
//   ERR2: interconnect used on the right-hand side of a continuous assign
//   ERR3: continuous assign between two interconnect nets
//   ERR4: force of an interconnect net (procedural continuous assignment)
//   ERR5: continuous assign to an interconnect port inside the module

module leaf(inout wire p);
endmodule

module m5(output interconnect y);
  // ERR5
  assign y = 1'b0;
endmodule

module top;
  interconnect w1, w2;
  interconnect w3, w4;
  wire y;
  // ERR1
  assign w1 = 1;
  // ERR2
  assign y = w2;
  // ERR3
  assign w3 = w4;
  // ERR4
  initial force w4 = 1'b1;
  m5 u5(y);
  leaf l1(w1);
  leaf l2(w2);
  leaf l3(w3);
  leaf l4(w4);
endmodule
