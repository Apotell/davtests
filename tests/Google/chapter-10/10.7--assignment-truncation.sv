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

// IEEE 1800-2023 Sec 10.7: a sized literal wider than its target loses its most significant bits.
module m;
  logic [3:0] x;
  logic [3:0] y;
  wire [3:0] w;
  logic [3:0] z = 8'hF0;
  assign w = 8'b1000_0001;
  initial begin
    x = 8'hFF;
    y = 8'h0F;
    x = 4'd9;
    x = 255;
    y = 12'o7777;
  end
endmodule
