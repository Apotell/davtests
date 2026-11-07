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
:name: ic_selects_in_connections
:description: selects and concatenations of interconnect nets used as port connections
:tags: 6.6.8
*/

// Checks in this file:
//   CHK1: bit select of an interconnect vector as a port connection
//   CHK2: part select of an interconnect vector as a port connection
//   CHK3: indexed part select w[+:] of an interconnect vector
//   CHK4: concatenation of interconnect nets only as a port connection
//   CHK5: element of a 2-D unpacked interconnect array as a port connection

module leaf1(inout wire p);
endmodule

module leaf2(inout wire [1:0] p);
endmodule

module leaf4(inout wire [3:0] p);
endmodule

module top;
  interconnect [7:0] w;
  interconnect [1:0] hi, lo;
  interconnect m [0:1][0:2];
  // CHK1
  leaf1 u1(w[0]);
  leaf1 u1b(w[1]);
  // CHK2
  leaf2 u2(w[3:2]);
  // CHK3
  leaf4 u3(w[4 +: 4]);
  // CHK4
  leaf4 u4({hi, lo});
  // CHK5
  for (genvar i = 0; i < 2; i++) begin : gi
    for (genvar j = 0; j < 3; j++) begin : gj
      leaf1 u5(m[i][j]);
    end
  end
endmodule
