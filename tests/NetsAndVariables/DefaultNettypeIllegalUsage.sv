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

// Catalog of illegal `default_nettype usages, as a companion to
// DefaultNettype.sv and DefaultNettypeNetTypes.sv (which cover the legal
// forms). Every construct below is illegal / would throw a compile error, so
// every one of them is shown only as a comment -- none of this file's active
// code depends on them, and the file compiles cleanly on its own.

module default_nettype_illegal_usage_test(
  input logic a,
  input logic b,
  output logic y
);
  // Legal baseline so this file has something that actually compiles.
  logic explicit_var;
  assign explicit_var = a ^ b;
  assign y = explicit_var;
endmodule

// ---------------------------------------------------------------------------
// ILLEGAL: `default_nettype only accepts a net type keyword (wire, tri,
// tri0, tri1, wand, wor, triand, trior, trireg, uwire, supply0, supply1) or
// the literal 'none' as its argument. A variable/data type keyword is not a
// net type and is not a valid argument here.
// ---------------------------------------------------------------------------
//   `default_nettype logic  // ILLEGAL: 'logic' is a data type, not a net type
//   `default_nettype reg    // ILLEGAL: 'reg' is a data type, not a net type
//   `default_nettype int    // ILLEGAL: 'int' is a data type, not a net type
//   `default_nettype bit    // ILLEGAL: 'bit' is a data type, not a net type
//   `default_nettype var    // ILLEGAL: 'var' is a declaration keyword, not a net type

// ---------------------------------------------------------------------------
// ILLEGAL: SystemVerilog keywords are case-sensitive. Capitalizing (or
// otherwise misspelling) a net type keyword makes it an unrecognized token,
// not the keyword itself.
// ---------------------------------------------------------------------------
//   `default_nettype Wire // ILLEGAL: 'Wire' is not the keyword 'wire'
//   `default_nettype WIRE // ILLEGAL: 'WIRE' is not the keyword 'wire'
//   `default_nettype None // ILLEGAL: 'None' is not the keyword 'none'

// ---------------------------------------------------------------------------
// ILLEGAL: `default_nettype requires exactly one argument; omitting it, or
// giving an identifier that is not a net type keyword or 'none' at all, is a
// compile error.
// ---------------------------------------------------------------------------
//   `default_nettype        // ILLEGAL: missing net-type argument
//   `default_nettype mytype // ILLEGAL: 'mytype' is not a net type or 'none'

// ---------------------------------------------------------------------------
// ILLEGAL: once `default_nettype none is in effect, ALL implicit-net
// contexts are disabled, not only the continuous-assignment case already
// shown in DefaultNettype.sv. An undeclared identifier used as an
// unconnected module instance port connection, or as a primitive gate
// terminal, is equally a compile error under 'none'.
// ---------------------------------------------------------------------------
//   `default_nettype none
//
//   module bad_instance_under_none(input logic a, input logic b, output logic y);
//     // ILLEGAL: 'undeclared_net' has no default net type to fall back on
//     some_other_module u_inst(.a(a), .b(b), .y(undeclared_net));
//
//     // ILLEGAL: same reason -- primitive gate terminals rely on implicit
//     // net creation just like continuous assignments and port connections do
//     and (undeclared_gate_net, a, b);
//   endmodule
