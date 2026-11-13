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

// 23.2.2.1 Example 1, output half: "port f inherits signed attribute from
// logic decl." and "logic g inherits signed attribute from port"; e and h
// have no explicit declaration, so they are nets: e unsigned, h signed.
module test(e, f, g, h);
  output [7:0] e;
  output [7:0] f;
  output signed [7:0] g;
  output signed [7:0] h;
  logic signed [7:0] f;
  logic [7:0] g;
endmodule
