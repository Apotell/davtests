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

program nets_and_variables_program(
  input wire a,
  input wire b,
  output logic y
);
  // Program scope behavior is the same in ANSI and non-ANSI style.
  // The only difference is the module port declaration syntax.

  wire prog_net; // legal net declaration in program scope
  logic prog_var; // legal variable declaration in program scope

  assign prog_net = a & b;

  initial begin
    prog_var = a ^ b;
    y = prog_var;
  end
endprogram
