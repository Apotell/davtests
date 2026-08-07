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

// Unlike modules, interfaces, and programs, a checker has no separate
// non-ANSI port style: checker_port_item (IEEE 1800 Annex A) always
// specifies direction and type inline in the port list. So this mirrors
// Ansi/Checker.sv's port declarations exactly; only the checker name
// differs, for uniqueness.
checker nets_and_variables_checker_nonansi(input logic clk, input logic a);
  // Checker scope variable declaration
  logic ck_logic;
  // Checker scope variable declaration
  logic [1:0] ck_vec;
  // Checker scope rand variable declaration
  rand bit [3:0] ck_rand;
  // Checker scope randc variable declaration
  randc bit [1:0] ck_randc;

  always_ff @(posedge clk) begin
    ck_logic <= a;
    ck_vec <= {ck_vec[0], a};
  end
endchecker
