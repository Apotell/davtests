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

// 23.2.3 Example 1: "Parameterized module declaration using non-ANSI style
// module header". in is added to the list of ports, which the printed
// example omits although it declares in as an input port.
module generic_fifo(clk, read, write, reset, in, out, full, empty);
  parameter MSB=3, LSB=0, DEPTH=4;
  input [MSB:LSB] in;
  input clk, read, write, reset;
  output [MSB:LSB] out;
  output full, empty;
  wire [MSB:LSB] in;
  wire clk, read, write, reset;
  logic [MSB:LSB] out;
  logic full, empty;
endmodule
