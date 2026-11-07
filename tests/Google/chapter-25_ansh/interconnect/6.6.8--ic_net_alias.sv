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
:name: ic_net_alias
:description: net_alias statements whose net_lvalues are all interconnect nets
:tags: 6.6.8
*/

// Checks in this file:
//   CHK1: alias between two scalar interconnect nets
//   CHK2: alias between two packed interconnect vectors
//   CHK3: alias between an interconnect vector and a concatenation of interconnects
//   CHK4: alias of part selects of interconnect vectors
//   CHK5: three interconnect nets in one alias statement

module leaf1(inout wire p);
endmodule

module leaf4(inout wire [3:0] p);
endmodule

module top;
  interconnect a, b;
  interconnect x, y;
  interconnect z;
  interconnect [3:0] v1, v2;
  interconnect [3:0] v3;
  interconnect [1:0] hi, lo;

  // CHK1
  alias a = b;
  // CHK2
  alias v1 = v2;
  // CHK3
  alias v3 = {hi, lo};
  // CHK4
  alias v1[1:0] = lo;
  // CHK5
  alias x = y = z;

  leaf1 l_a(a);
  leaf4 l_v1(v1);
  leaf4 l_v3(v3);
  leaf1 l_x(x);
endmodule
