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

// 23.2.2.2: ANSI style explicitly named ports. "The self-determined type of
// the port expression becomes the type for the port." Adapted from the
// mymod example.
module mymod(output .P1(r[3:0]),
             output .P2(r[7:4]),
             ref .Y(x),
             input R);
  logic [7:0] r;
  int x;
endmodule
