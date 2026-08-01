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

// Demonstrates the `default_nettype compiler directive, which controls what
// net type an implicitly declared net gets (or disables implicit nets
// entirely with 'none'). Constructs that are illegal / would throw a compile
// error are shown only as comments, since including them as live code would
// prevent this file from compiling.

`default_nettype wire // this is the language default; set explicitly here for clarity

module default_nettype_wire_test(a, b, y);
  input a;
  input b;
  output y;

  // Legal: an undeclared identifier used as a continuous assignment target
  // becomes an implicit net of the current default_nettype (wire here).
  assign implicit_net_wire = a & b; // implicit wire, legal under default_nettype wire

  assign y = implicit_net_wire ^ a;
endmodule

`default_nettype tri

module default_nettype_tri_test(a, b, y);
  input a;
  input b;
  output y;

  // Legal: same implicit-net mechanism, but the implicit net is now a 'tri'
  // net because of the preceding `default_nettype tri directive.
  assign implicit_net_tri = a | b; // implicit tri, legal under default_nettype tri

  assign y = implicit_net_tri;
endmodule

`default_nettype none

module default_nettype_none_test(
  input logic a,
  input logic b,
  output logic y
);
  // Legal: with `default_nettype none, every identifier must be explicitly
  // declared. ANSI ports here already carry an explicit type (logic), so
  // they are unaffected by 'none'.
  logic explicit_var;
  assign explicit_var = a ^ b;
  assign y = explicit_var;

  // ILLEGAL: with `default_nettype none in effect, an undeclared identifier
  // can no longer be implicitly declared as a net. This is a compile error
  // (identifier has not been declared / no default net type).
  //   assign implicit_net_none = a & b; // ILLEGAL: no default net type in effect

endmodule

// ILLEGAL (shown only as a comment; would need its own file to demonstrate,
// since it would prevent this file from compiling): a non-ANSI module whose
// ports have no explicit type cannot rely on implicit net creation either
// while `default_nettype none is in effect.
//   module bad_nonansi_under_none(a, b, y);
//     input a;   // ILLEGAL: port 'a' has no net type and 'none' is in effect
//     input b;   // ILLEGAL: port 'b' has no net type and 'none' is in effect
//     output y;  // ILLEGAL: port 'y' has no net type and 'none' is in effect
//   endmodule

// Restore the language default before the end of the file. Per the LRM,
// `default_nettype reverts to 'wire' at the start of each new source file
// (or compilation unit) anyway, but explicitly resetting it here documents
// intent and protects any file that might later be appended after this one.
`default_nettype wire
