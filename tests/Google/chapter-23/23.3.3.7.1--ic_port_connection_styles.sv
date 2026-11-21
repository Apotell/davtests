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
:name: ic_port_connection_styles
:description: interconnect nets connected to ports with all connection styles and through hierarchy
:tags: 23.3.3.7.1
*/

// Checks in this file:
//   CHK1: interconnect net connected by position
//   CHK2: interconnect net connected by name
//   CHK3: interconnect net connected with .name
//   CHK4: interconnect net connected with .*
//   CHK5: interconnect net -> interconnect port -> wire port (two levels)

module leaf(inout wire p);
endmodule

module mid(inout interconnect p);
  leaf u_leaf(.p(p));
endmodule

module top;
  interconnect n1, n2;
  interconnect p, n5;
  // CHK1
  leaf u1(n1);
  // CHK2
  leaf u2(.p(n2));
  // CHK3
  leaf u3(.p);
  // CHK4
  mid u4(.*);
  // CHK5
  mid u5(n5);
endmodule
