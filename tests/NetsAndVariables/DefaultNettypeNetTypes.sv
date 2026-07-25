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

// Companion to DefaultNettype.sv, which only demonstrates 'wire', 'tri' and
// 'none'. This file completes legal `default_nettype coverage with the
// remaining net type keywords the LRM allows as its argument: wand, wor,
// tri0, tri1, triand, trior, uwire, supply0/supply1 and trireg.

`default_nettype wand

module default_nettype_wand_test(a, b, y);
  input a;
  input b;
  output y;

  // Legal: implicit net takes the current default_nettype (wand here).
  assign implicit_net_wand = a & b; // implicit wand, legal under default_nettype wand
  assign y = implicit_net_wand;
endmodule

`default_nettype wor

module default_nettype_wor_test(a, b, y);
  input a;
  input b;
  output y;

  assign implicit_net_wor = a | b; // implicit wor, legal under default_nettype wor
  assign y = implicit_net_wor;
endmodule

`default_nettype tri0

module default_nettype_tri0_test(a, b, y);
  input a;
  input b;
  output y;

  assign implicit_net_tri0 = a ^ b; // implicit tri0, legal under default_nettype tri0
  assign y = implicit_net_tri0;
endmodule

`default_nettype tri1

module default_nettype_tri1_test(a, b, y);
  input a;
  input b;
  output y;

  assign implicit_net_tri1 = a & b; // implicit tri1, legal under default_nettype tri1
  assign y = implicit_net_tri1;
endmodule

`default_nettype triand

module default_nettype_triand_test(a, b, y);
  input a;
  input b;
  output y;

  assign implicit_net_triand = a | b; // implicit triand, legal under default_nettype triand
  assign y = implicit_net_triand;
endmodule

`default_nettype trior

module default_nettype_trior_test(a, b, y);
  input a;
  input b;
  output y;

  assign implicit_net_trior = a ^ b; // implicit trior, legal under default_nettype trior
  assign y = implicit_net_trior;
endmodule

`default_nettype uwire

module default_nettype_uwire_test(a, b, y);
  input a;
  input b;
  output y;

  assign implicit_net_uwire = a & b; // implicit uwire, legal under default_nettype uwire
  assign y = implicit_net_uwire;
endmodule

`default_nettype trireg

module default_nettype_trireg_test(a, b, y);
  input a;
  input b;
  output y;

  assign implicit_net_trireg = a | b; // implicit trireg, legal under default_nettype trireg
  assign y = implicit_net_trireg;
endmodule

// supply0/supply1 are unusual as an implicit-net default: an implicitly
// created supply0 net has no way to receive its constant driving strength
// the way an explicit 'supply0 foo;' declaration normally would, but the net
// type itself is still a legal `default_nettype value.
`default_nettype supply0

module default_nettype_supply0_test(a, b, y);
  input a;
  input b;
  output y;

  assign implicit_net_supply0 = a ^ b; // implicit supply0, legal under default_nettype supply0
  assign y = implicit_net_supply0;
endmodule

// Restore the language default before the end of the file (see DefaultNettype.sv).
`default_nettype wire
