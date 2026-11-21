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
:name: mp_err_selection_mismatch
:description: wrong or inconsistent modport selection
:should_fail_because: the selected modport must exist and be identical in header and instance for the same interface type
:tags: 25.5
*/

// Errors in this file:
//   ERR1: module header selects a modport that the interface does not have
//   ERR2: instance connection selects a modport that the interface does not have
//   ERR3: header and instance select two different modports
//   ERR4: header modport of interface if_a connected to a modport of interface if_b
//   ERR5: modport selected on a plain signal instead of an interface

interface if_a;
  logic x;
  modport mp (input x);
  modport other (output x);
endinterface

interface if_b;
  logic x;
  modport mp (input x);
endinterface

// ERR1
module m1(if_a.nomp p);
endmodule

module m2(if_a p);
endmodule

module m3(if_a.mp p);
endmodule

module m4(if_a.mp p);
endmodule

module m5(if_a p);
endmodule

module top;
  if_a ia();
  if_b ib();
  logic sig;
  m1 u1(ia);
  // ERR2
  m2 u2(ia.nomp);
  // ERR3
  m3 u3(ia.other);
  // ERR4
  m4 u4(ib.mp);
  // ERR5
  m5 u5(sig.mp);
endmodule
