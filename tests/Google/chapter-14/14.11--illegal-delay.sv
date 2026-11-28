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

// IEEE 1800-2023 Sec 14.11 and 14.16.1: cycle delays need a default clocking; no time delay in a synchronous drive.
module no_def(input logic clk);
  initial begin
    ##5;
  end
endmodule

module with_def(input logic clk, input logic [7:0] r);
  logic [7:0] data;
  default clocking cb @(posedge clk);
    output data;
  endclocking
  initial begin
    ##2;
    cb.data <= ##2 r;
    cb.data <= #4 r;
    data <= #4 r;
  end
endmodule
