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

// 23.2.2.3 example mh22: "p_b is an explicitly named port that inherits only
// the direction input from port p_a. Its data type is that of s_b. Port p_c
// inherits only the direction from p_b, and defaults to the net port kind
// and to the logic data type."
module mh22(input wire integer p_a, .p_b(s_b), p_c);
  logic [5:0] s_b;
endmodule
