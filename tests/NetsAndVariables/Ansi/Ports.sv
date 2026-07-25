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
  input logic clk, // explicit variable port: 'logic' keyword given explicitly
  input logic a, // explicit variable port: 'logic' keyword given explicitly
  input logic b, // explicit variable port: 'logic' keyword given explicitly
  input c, // implicit net port: no type keyword given, defaults to a net (wire) under default_nettype
  input wire d, // explicit net port: 'wire' keyword given explicitly
  output logic y // explicit variable port: 'logic' keyword given explicitly
);
endmodule
