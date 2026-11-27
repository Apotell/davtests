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

// IEEE 1800-2023 Sec 16.13.1: differently clocked sequences may only be combined with ##1 and ##0.
module m(input logic clk0, clk1, clk2, sig0, sig1, a, b);
  sequence s_concat1; @(posedge clk0) sig0 ##1 @(posedge clk1) sig1; endsequence
  sequence s_concat0; @(posedge clk0) sig0 ##0 @(posedge clk1) sig1; endsequence
  sequence s_delay2; @(posedge clk1) a ##2 @(posedge clk2) b; endsequence
  sequence s_intersect; @(posedge clk1) a intersect @(posedge clk2) b; endsequence
  sequence s_within; (@(posedge clk1) a) within (@(posedge clk2) b); endsequence
  sequence s_range; @(posedge clk1) a ##[1:3] @(posedge clk2) b; endsequence
  sequence s_same_clock; @(posedge clk1) a ##2 @(posedge clk1) b; endsequence
  property p_and; (@(posedge clk0) sig0) and (@(posedge clk1) sig1); endproperty
  A1: assert property (@(posedge clk1) a intersect @(posedge clk2) b);
endmodule
