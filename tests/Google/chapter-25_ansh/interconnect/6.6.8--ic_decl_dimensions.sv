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
:name: ic_decl_dimensions
:description: interconnect nets with different packed and unpacked dimension styles
:tags: 6.6.8
*/

// Checks in this file:
//   CHK1: ascending packed range interconnect [0:7]
//   CHK2: multiple packed dimensions interconnect [1:0][3:0]
//   CHK3: C-style unpacked size interconnect w [4]
//   CHK4: two unpacked dimensions interconnect w [0:3][0:1]
//   CHK5: each interconnect declared on its own line vs list with unpacked dims

module sink1(input wire p);
endmodule

module sink8(input wire [0:7] p);
endmodule

module sink24(input wire [1:0][3:0] p);
endmodule

module top;
  // CHK1
  interconnect [0:7] asc;
  // CHK2
  interconnect [1:0][3:0] p2;
  // CHK3
  interconnect csz [4];
  // CHK4
  interconnect two [0:3][0:1];
  // CHK5
  interconnect e0;
  interconnect e1;
  interconnect l0 [2], l1 [2];

  sink8  s_asc(asc);
  sink24 s_p2(p2);
  for (genvar i = 0; i < 4; i++) begin : g
    sink1 s_c(csz[i]);
    sink1 s_t0(two[i][0]);
    sink1 s_t1(two[i][1]);
  end
  sink1 s_e0(e0);
  sink1 s_e1(e1);
  sink1 s_l00(l0[0]);
  sink1 s_l01(l0[1]);
  sink1 s_l10(l1[0]);
  sink1 s_l11(l1[1]);
endmodule
