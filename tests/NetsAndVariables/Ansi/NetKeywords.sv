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

  // Net declarations
  wire w0; // net declaration
  tri t0; // net declaration
  tri0 t0z; // net declaration
  tri1 t1z; // net declaration
  wand wand_net; // net declaration
  wor wor_net; // net declaration
  triand triand_net; // net declaration
  trior trior_net; // net declaration
  supply0 supply0_net; // net declaration
  supply1 supply1_net; // net declaration
  uwire uwire_net; // net declaration
  wire [7:0] w_bus; // net declaration
  tri [3:0] tri_bus; // net declaration

  assign w0 = a & b;
  assign w_bus[0] = a;
  assign uwire_net = a ^ b;

endmodule
