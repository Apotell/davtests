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

// White space between the apostrophe and the base of a based number (IEEE 1800-2023 Sec 5.7.1).

module top;
  logic [7:0] a, b, c, d, e;
  assign a = 8 'hFF;
  assign b = 8'h FF;
  assign c = 8' hFF;
  assign d = 8 ' h FF;
  assign e = 8's hFF;
endmodule
