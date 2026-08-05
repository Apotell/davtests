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

// Illegal construct: a procedural (blocking) assignment to an undeclared
// identifier inside an 'always' block. Per IEEE 1800 clause 6.10, implicit
// declaration is net-only and only arises from a continuous assignment, an
// unconnected port connection, or a primitive gate terminal -- never from a
// procedural assignment. There is no such thing as an "implicit variable"
// in SystemVerilog.

module illegal_construct_procedural_assignment_undeclared_test(input logic clk, input logic a);
  always @(posedge clk) begin
    undeclared_var = a; // ILLEGAL: undeclared identifier assigned procedurally
  end
endmodule
