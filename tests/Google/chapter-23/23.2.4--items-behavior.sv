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

// 23.2.4, Syntax 23-5: module_common_item includes net_alias,
// continuous_assign, initial_construct, final_construct and always_construct.
module m(input logic clk, input logic d);
  wire n1, n2;
  alias n1 = n2;
  wire w;
  assign w = d;
  logic a, b, c, e, f;
  initial a = 1'b0;
  final a = 1'b1;
  always @(posedge clk) b <= d;
  always_comb c = d;
  always_ff @(posedge clk) e <= d;
  always_latch if (clk) f <= d;
endmodule
