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

// IEEE 1800-2023 Sec 23.11: bind target scope kind and instance name clashes.
module target(input logic clk, a);
  logic sig;
  logic u_clash;
endmodule

interface itf(input logic clk);
endinterface

program prg(input logic clk);
endprogram

module chk_mod(input logic c, d);
endmodule

module top(input logic clk, a);
  target t1(clk, a);
  itf i1(clk);
  prg p1(clk);
endmodule

bind target chk_mod u_ok(clk, a);
bind itf chk_mod u_itf(clk, clk);
bind prg chk_mod u_prg(clk, clk);
bind target chk_mod u_clash(clk, a);
bind target chk_mod sig(clk, a);
