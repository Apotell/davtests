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

// IEEE 1800-2023 Sec 16.16: a concurrent assertion shall have a clock.
module m_unclocked(input logic clk, a, b);
  A_PLAIN: assert property (a |-> b);
  C_PLAIN: cover property (a ##1 b);
  initial begin
    A_INIT: assert property (a);
  end
endmodule

module m_clocked(input logic clk, a, b);
  property p_clk; @(posedge clk) a |-> b; endproperty
  A_EXPL: assert property (@(posedge clk) a |-> b);
  A_NAMED: assert property (p_clk);
  always @(posedge clk) begin
    A_PROC: assert property (a |-> b);
  end
endmodule

module m_default(input logic clk, a, b);
  default clocking cb @(posedge clk); endclocking
  A_DEF: assert property (a |-> b);
endmodule

module m_named_unclocked(input logic a, b);
  property p_noclk; a |-> b; endproperty
  A_PROP: assert property (p_noclk);
endmodule
