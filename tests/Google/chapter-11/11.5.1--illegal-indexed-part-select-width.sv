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

// Indexed part-selects whose width is not a positive constant (IEEE 1800-2023 Sec 11.5.1).

module top;
  logic [7:0] a, b;
  int i;
  initial begin
    b[3:0] = a[i+:0];
    b[3:0] = a[i-:-2];
    b[3:0] = a[i+:4'd0];
    b[3:0] = a[i+:1'bx];
    b[3:0] = a[i+:4];
    b[2:0] = a[2-:3];
  end
endmodule
