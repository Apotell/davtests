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
:name: mp_select_in_header
:description: modport selected in the module header and connected in several ways
:tags: 25.5
*/

// Checks in this file:
//   CHK1: header i2.initiator connected by name
//   CHK2: header i2.target connected by position
//   CHK3: header modport connected with .name
//   CHK4: header modport connected with .*
//   CHK5: LRM i2 example with wire signals

interface i2;
  wire a, b, c, d;
  modport initiator (input a, b, output c, d);
  modport target (output a, b, input c, d);
endinterface

module m(i2.initiator i);
endmodule

module s(i2.target i);
endmodule

module top;
  // CHK5
  i2 i();
  // CHK1
  m u1(.i(i));
  // CHK2
  s u2(i);
  // CHK3
  m u3(.i);
  // CHK4
  s u4(.*);
endmodule
