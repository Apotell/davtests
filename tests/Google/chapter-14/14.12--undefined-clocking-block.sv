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

// IEEE 1800-2023 Sec 14.12: "default clocking <name>;" shall name a clocking block of the scope.
module m1(input logic clk, input logic a);
  default clocking nocb;
  default clocking later;
  clocking later @(posedge clk);
    input a;
  endclocking
endmodule

module m2(input logic clk);
  clocking other @(posedge clk); endclocking
endmodule

module m3;
  default clocking other;
endmodule

interface i1(input logic clk);
  clocking icb @(posedge clk); endclocking
  default clocking icb;
  default clocking inone;
endinterface

checker ck(logic clk);
  clocking ccb @(posedge clk); endclocking
  default clocking ccb;
  default clocking cnone;
endchecker
