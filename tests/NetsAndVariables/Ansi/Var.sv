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

// Demonstrates the SystemVerilog 'var' keyword, which explicitly declares a
// variable (never a net). Constructs that are illegal / would throw a
// compile error are shown only as comments, since including them as live
// code would prevent this file from compiling.

module var_keyword_test(
  input logic clk,
  input logic a,
  input logic b,
  output logic y
);

  // Legal: 'var' with an explicit data type -- an ordinary variable declaration.
  var logic var_logic; // legal variable declaration
  var bit var_bit; // legal variable declaration
  var int var_int; // legal variable declaration
  var byte var_byte; // legal variable declaration

  // Legal: 'var' with no data type -- the type defaults to 'logic'.
  var var_implicit_logic; // legal, equivalent to 'var logic var_implicit_logic;'

  // Legal: 'var' with a packed vector type.
  var logic [3:0] var_vector; // legal packed vector variable

  // Legal: 'var' declaring a variable with an initial value.
  var logic var_initialized = 1'b0; // legal variable declaration with initial value

  // Legal: a continuous assignment may drive a variable (not just a net).
  assign var_initialized = a & b; // legal: SystemVerilog allows assign to target a variable

  // ILLEGAL: 'var' combined with a net type keyword.
  // 'var' requires a data type, and net type keywords (wire, tri, wand, wor,
  // tri0, tri1, triand, trior, supply0, supply1, uwire) are not data types --
  // they are net types. Combining them is a syntax/compile error.
  //   var wire var_wire;       // ILLEGAL: 'wire' is a net type, not a data type
  //   var tri var_tri;         // ILLEGAL: 'tri' is a net type, not a data type
  //   var supply0 var_supply0; // ILLEGAL: 'supply0' is a net type, not a data type

  // ILLEGAL: reversing the keyword order does not make it legal either.
  //   wire var var_wire2; // ILLEGAL: still mixes a net type with 'var'

  // ILLEGAL: redeclaring the same identifier with a net-type declaration
  // after it was already declared with 'var' -- illegal identifier redeclaration.
  //   wire var_logic; // ILLEGAL: 'var_logic' is already declared as a variable above

  always_comb begin
    var_logic = a ^ b;
    var_bit = a & b;
  end

  always_ff @(posedge clk) begin
    var_int <= var_int + 1;
  end

  assign y = var_logic;

endmodule
