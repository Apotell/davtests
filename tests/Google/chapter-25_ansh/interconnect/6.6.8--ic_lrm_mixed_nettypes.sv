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
:name: ic_lrm_mixed_nettypes
:description: LRM example: interconnect array whose elements resolve to different net types
:tags: 6.6.8
*/

// Checks in this file:
//   CHK1: package with nettype real realNet
//   CHK2: iBus[0] connected to a wire logic output
//   CHK3: iBus[1] connected to a realNet output
//   CHK4: whole interconnect array passed to input interconnect iBus[0:1] port
//   CHK5: interconnect port elements forwarded to logic and real modules

// CHK1
package NetsPkg;
  nettype real realNet;
endpackage : NetsPkg

module top();
  interconnect iBus[0:1];
  // CHK2
  lDriver l1(iBus[0]);
  // CHK3
  rDriver r1(iBus[1]);
  // CHK4
  rlMod m1(iBus);
endmodule : top

module lDriver(output wire logic out);
endmodule : lDriver

module rDriver
  import NetsPkg::*;
  (output realNet out);
endmodule : rDriver

module rlMod(input interconnect iBus[0:1]);
  // CHK5
  lMod l1(iBus[0]);
  rMod r1(iBus[1]);
endmodule : rlMod

module lMod(input wire logic in);
endmodule : lMod

module rMod
  import NetsPkg::*;
  (input realNet in);
endmodule : rMod
