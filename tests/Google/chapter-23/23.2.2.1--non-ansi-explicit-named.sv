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

// 23.2.2.1 Example 5: explicitly named ports .a and .g connected to a
// concatenation and a bit-select of internal nets, plus one implicit port f.
module renamed_concat(.a({b, c}), f, .g(h[1]));
  input b, c;
  input f;
  input [1:0] h;
endmodule
