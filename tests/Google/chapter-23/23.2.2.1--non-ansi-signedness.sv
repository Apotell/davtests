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

// 23.2.2.1: "If either the port or the net/variable is declared as signed,
// then the other shall also be considered signed." "Nets connected to ports
// without an explicit net declaration shall be considered unsigned, unless
// the port is declared as signed." Adapted from Example 1.
module test(a, b, c, d);
  input [7:0] a;
  input [7:0] b;
  input signed [7:0] c;
  input signed [7:0] d;
  wire signed [7:0] b;
  wire [7:0] c;
endmodule
