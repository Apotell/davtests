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

  // Variable declarations
  logic var_logic; // variable declaration
  reg var_reg; // variable declaration

  bit var_bit; // variable declaration
  int var_int; // variable declaration
  integer var_integer; // variable declaration
  shortint var_shortint; // variable declaration
  longint var_longint; // variable declaration
  byte var_byte; // variable declaration
  time var_time; // variable declaration
  real var_real; // variable declaration
  realtime var_realtime; // variable declaration
  string var_string; // variable declaration
  chandle var_chandle; // variable declaration
  event var_event; // variable declaration
  enum logic [1:0] {IDLE, BUSY} var_enum; // variable declaration
  logic [3:0] var_vector; // variable declaration
  reg [7:0] var_reg_vector; // variable declaration

endmodule
