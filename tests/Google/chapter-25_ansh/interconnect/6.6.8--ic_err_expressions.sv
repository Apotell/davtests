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
:name: ic_err_expressions
:description: interconnect nets used in expressions other than all-interconnect net_lvalues
:should_fail_because: an interconnect net shall only appear in net_lvalue expressions whose nets are all interconnect
:tags: 6.6.8
*/

// Errors in this file:
//   ERR1: negated interconnect as a port connection .p(~w)
//   ERR2: arithmetic on an interconnect as a port connection .p(w + 1)
//   ERR3: concatenation mixing an interconnect and a wire as a port connection
//   ERR4: interconnect inside a conditional expression as a port connection
//   ERR5: alias between an interconnect and a wire

module leaf1(input wire p);
endmodule

module leaf2(input wire [1:0] p);
endmodule

module top;
  interconnect w1, w2;
  interconnect w3, w4;
  interconnect w5;
  wire wr, s, a, b;
  // ERR1
  leaf1 u1(.p(~w1));
  // ERR2
  leaf2 u2(.p(w2 + 1));
  // ERR3
  leaf2 u3(.p({w3, wr}));
  // ERR4
  leaf1 u4(.p(s ? w4 : a));
  // ERR5
  alias w5 = b;
endmodule
