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

// Checkers instantiated in a fork block or in a procedure of another checker (IEEE 1800-2023 Sec 17.3).

checker c(bit a); endchecker
checker outer(bit a, untyped clk);
  always_ff @clk begin
    c i1(a);
  end
endchecker
module m;
  bit x;
  initial fork
    c inst(x);
  join
  initial begin
    c ok(x);
  end
endmodule
