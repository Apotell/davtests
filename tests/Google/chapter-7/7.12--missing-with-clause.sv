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

// Array manipulation methods that need a with clause (IEEE 1800-2023 Sec 7.12, 7.12.1, 7.12.5).

module top;
  int IA[int], qi[$], A[];
  int s;
  initial begin
    qi = IA.find(x);
    qi = IA.find_index;
    A = A.map();
    s = A.sum(z);
    qi = IA.find with (item > 2);
    qi = IA.find(y) with (y > 2);
    A = A.map(e) with (e + 1);
    s = A.sum;
    s = A.sum(w) with (w * 2);
    qi = IA.unique;
  end
endmodule
