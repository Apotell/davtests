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

// IEEE 1800-2023 Sec 17.2 and 17.7.1: illegal items in a checker.
checker ck(input logic clk, input logic in1, output logic out1);
  bit v;
  always_ff @(posedge clk) begin
    v = in1;
    v <= in1;
  end
  always_comb in1 = 1'b0;
  assign out1 = v;
endchecker

module top(input logic clk, a);
  logic o;
  ck c1(clk, a, o);
endmodule

checker ck_free(input logic clk, input logic in1);
  rand bit fr;
  assign fr = in1;
  always_ff @(posedge clk) fr <= in1;
endchecker
