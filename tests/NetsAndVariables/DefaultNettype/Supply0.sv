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
// 'none'. This file covers the 'supply0' net type keyword as a
// `default_nettype value.
//
// supply0 is unusual as an implicit-net default: an implicitly created
// supply0 net has no way to receive its constant driving strength the way
// an explicit 'supply0 foo;' declaration normally would, but the net type
// itself is still a legal `default_nettype value.

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
