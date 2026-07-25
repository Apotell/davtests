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

module nets_and_variables_test(
  input logic clk,
  input logic a,
  input logic b,
  output logic y
);

  // Implicit declarations
  // In SystemVerilog, undeclared identifiers used in assignments may be implicitly declared
  // as nets or variables depending on context, and these examples are intended to exercise that.
  wire explicit_wire; // explicit net declaration via wire keyword
  logic explicit_logic; // explicit variable declaration, not implicit

  // Implicit net examples from use before declaration
  assign implicit_wire = a | b; // continuous assignment to an implicit net-like object
  assign implicit_net_a = a & b; // implicit net declaration by continuous assignment
  assign implicit_net_b = implicit_net_a | a; // implicit net declaration by continuous assignment

  // "Implicit variable" examples -- per LRM 6.10, implicit declarations are
  // net-only; there is no implicit variable in SystemVerilog. Ideally it should cause compile errors.
 
  //Assigning to an undeclared identifier in a procedural block is included here to exercise
  // what surelog actually does with it 
  always @(posedge clk) begin
    implicit_var_a = a ^ b; // undeclared identifier assigned procedurally
    implicit_var_b = implicit_var_a; // undeclared identifier assigned procedurally
  end

endmodule
