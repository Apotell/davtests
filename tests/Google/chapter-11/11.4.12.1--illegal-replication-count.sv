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

// Replications whose multiplier is not a non-negative, non-x, non-z constant (IEEE 1800-2023 Sec 11.4.12.1).

module top;
  parameter P = 2;
  logic [7:0] a, b, c, d, e;
  assign a = {1'bz{1'b0}};
  assign b = {1'bx{1'b0}};
  assign c = {-1{1'b0}};
  assign d = {4{2'b01}};
  assign e = {P{4'h3}};
endmodule
