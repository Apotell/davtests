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

// IEEE 1800-2023 Sec 14.12 and 14.14: at most one default clocking and one global clocking per scope.
module m_two_default(input logic clk);
  default clocking cb1 @(posedge clk); endclocking
  default clocking cb2 @(negedge clk); endclocking
endmodule

module m_two_global(input logic clk);
  global clocking g1 @(posedge clk); endclocking
  global clocking g2 @(negedge clk); endclocking
endmodule

module m_decl_then_ref(input logic clk);
  default clocking d1 @(posedge clk); endclocking
  clocking other @(negedge clk); endclocking
  default clocking other;
endmodule

module m_ref_then_decl(input logic clk);
  clocking c0 @(posedge clk); endclocking
  default clocking c0;
  default clocking late @(negedge clk); endclocking
endmodule

module m_one_each(input logic clk);
  default clocking dd @(posedge clk); endclocking
  global clocking gg @(posedge clk); endclocking
endmodule

interface i_two_default(input logic clk);
  default clocking ia @(posedge clk); endclocking
  default clocking ib @(negedge clk); endclocking
endinterface

program p_two_default(input logic clk);
  default clocking pa @(posedge clk); endclocking
  default clocking pb @(negedge clk); endclocking
endprogram

checker k_two(logic clk);
  default clocking ka @(posedge clk); endclocking
  default clocking kb @(negedge clk); endclocking
  global clocking kg1 @(posedge clk); endclocking
  global clocking kg2 @(negedge clk); endclocking
endchecker
