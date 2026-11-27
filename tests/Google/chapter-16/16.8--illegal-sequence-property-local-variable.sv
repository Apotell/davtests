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

// IEEE 1800-2023 Sec 16.8, 16.10, 16.12.1, 16.12.11, 16.12.14 and 16.12.16.
module m(input logic clk, a, b, c, input int data);
  sequence s_cyc1; a ##1 s_cyc2; endsequence
  sequence s_cyc2; b ##1 s_cyc1; endsequence
  sequence s_self; a ##1 s_self; endsequence
  sequence s_ok; a ##1 b; endsequence
  sequence s_use; s_ok ##1 c; endsequence

  property p_salw_bad; s_always [2:$] a; endproperty
  property p_salw_ok; s_always [2:5] a; endproperty
  property p_alw_ok; always [2:$] a; endproperty

  property p_case(x);
    case (x)
      1'b0: a;
      default: b;
      default: c;
    endcase
  endproperty

  sequence s_lv_clk; int v; (a, v = data) ##1 @(posedge v) b; endsequence
  property p_lv_dis; int v; disable iff (v == 0) (a, v = data) |=> b; endproperty
  property p_lv_abort; int v; (a, v = data) |=> accept_on (v == 1) b; endproperty
  property p_lv_ok; int v; (a, v = data) |=> (b == v); endproperty

  A1: assert property (@(posedge clk) p_lv_ok);
endmodule
