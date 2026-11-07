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
:name: ic_user_nettype
:description: interconnect nets resolving to user-defined nettypes
:tags: 6.6.8
*/

// Checks in this file:
//   CHK1: interconnect connected to a nettype real port
//   CHK2: interconnect connected to a nettype with a resolution function
//   CHK3: interconnect driven by a realNet output and read by a realNet input
//   CHK4: interconnect port forwarding a nettype net to a child
//   CHK5: same interconnect-ported module reused with wire and with nettype

package NetsPkg;
  nettype real realNet;
  function automatic real rsum(input real drivers[]);
    rsum = 0.0;
    foreach (drivers[i]) rsum += drivers[i];
  endfunction
  nettype real sumNet with rsum;
endpackage

import NetsPkg::*;

module rdrv(output realNet o);
  assign o = 1.5;
endmodule

module rrcv(input realNet i);
endmodule

module sdrv(output sumNet o);
  assign o = 2.0;
endmodule

module wdrv(output wire o);
  assign o = 1'b1;
endmodule

module pass(inout interconnect p);
  rrcv u_r(p);
endmodule

module generic_pass(inout interconnect p);
endmodule

module top;
  // CHK1
  interconnect n1;
  rrcv u1(n1);
  // CHK2
  interconnect n2;
  sdrv u2a(n2);
  sdrv u2b(n2);
  // CHK3
  interconnect n3;
  rdrv u3a(n3);
  rrcv u3b(n3);
  // CHK4
  interconnect n4;
  rdrv u4a(n4);
  pass u4b(n4);
  // CHK5
  interconnect n5r, n5w;
  rdrv u5a(n5r);
  generic_pass u5b(n5r);
  wdrv u5c(n5w);
  generic_pass u5d(n5w);
endmodule
