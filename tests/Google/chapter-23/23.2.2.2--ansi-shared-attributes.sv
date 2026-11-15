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

// 23.2.2.2: the ANSI version of 23.2.2.1 Example 1. "Multiple ports that
// share all attributes can be declared together." "Every attribute of the
// declaration shall be in the one declaration."
module test(input [7:0] a,
            input signed [7:0] b, c, d,
            output [7:0] e,
            output var signed [7:0] f, g,
            output signed [7:0] h);
endmodule
