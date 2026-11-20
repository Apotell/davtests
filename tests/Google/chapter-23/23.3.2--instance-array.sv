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

// 23.3.2: "The instantiations of modules can contain a range specification.
// This allows an array of instances to be created." (28.3.5, 37.11)
module cell_m (input i, output o);
endmodule

module top;
  wire [3:0] a, y;
  cell_m u [3:0] (.i(a), .o(y));
endmodule
