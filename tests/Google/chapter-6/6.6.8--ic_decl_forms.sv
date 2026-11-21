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
:name: ic_decl_forms
:description: legal interconnect net declaration forms from 6.7.1
:tags: 6.6.8
*/

// Checks in this file:
//   CHK1: scalar interconnect w1
//   CHK2: packed vector interconnect [3:0] w2
//   CHK3: packed + unpacked interconnect [3:0] w3 [1:0]
//   CHK4: two interconnect nets in one declaration: interconnect a, b (grammar allows at most two)
//   CHK5: interconnect with a single delay value #5

module sink1(input wire p);
endmodule

module sink4(input wire [3:0] p);
endmodule

module top;
  // CHK1
  interconnect w1;
  // CHK2
  interconnect [3:0] w2;
  // CHK3
  interconnect [3:0] w3 [1:0];
  // CHK4
  interconnect a, b;
  interconnect c;
  // CHK5
  interconnect #5 d;

  sink1 s_w1(w1);
  sink4 s_w2(w2);
  sink4 s_w30(w3[0]);
  sink4 s_w31(w3[1]);
  sink1 s_a(a);
  sink1 s_b(b);
  sink1 s_c(c);
  sink1 s_d(d);
endmodule
