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
:name: mp_select_at_instance
:description: modport selected in the port connection of the instance
:tags: 25.5
*/

// Checks in this file:
//   CHK1: .i(i.initiator) named connection with modport
//   CHK2: positional connection i.target
//   CHK3: same modport selected in both header and instance (identical, legal)
//   CHK4: no modport anywhere - full access to all signals
//   CHK5: two instances of the same module getting different modports

interface i2;
  wire a, b, c, d;
  modport initiator (input a, b, output c, d);
  modport target (output a, b, input c, d);
endinterface

module m(i2 i);
endmodule

module m_hdr(i2.initiator i);
endmodule

module full(i2 i);
  assign i.a = i.c;
endmodule

module top;
  i2 i();
  // CHK1
  m u1(.i(i.initiator));
  // CHK2
  m u2(i.target);
  // CHK3
  m_hdr u3(.i(i.initiator));
  // CHK4
  full u4(.i(i));
  // CHK5
  m u5(i.initiator);
  m u6(i.target);
endmodule
