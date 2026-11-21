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
:name: ic_err_unresolved
:description: interconnect nets that remain interconnect at the end of elaboration
:should_fail_because: it shall be illegal for a simulated net to still be an interconnect net at the end of elaboration
:tags: 23.3.3.7.1
*/

// Errors in this file:
//   ERR1: interconnect net connected only to an interconnect port
//   ERR2: three-level chain where every port is interconnect
//   ERR3: interconnect array element connected only to an interconnect port
//   ERR4: two interconnect nets aliased with no typed connection

module ic_leaf(inout interconnect p);
endmodule

module ic_mid(inout interconnect p);
  ic_leaf u(p);
endmodule

module top;
  interconnect n1, n2;
  interconnect a, b;
  interconnect arr [0:1];
  // ERR1
  ic_leaf u1(n1);
  // ERR2
  ic_mid u2(n2);
  // ERR3
  ic_leaf u3(arr[0]);
  ic_leaf u4(arr[1]);
  // ERR4
  alias a = b;
endmodule
