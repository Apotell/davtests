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

// new[] applied to a variable that is not a dynamic array (IEEE 1800-2023 Sec 7.5.1).

module top;
  int dyn[];
  int fixed[4];
  int q[$];
  int assoc[string];
  int scalar;
  initial begin
    dyn = new[4];
    dyn = new[8](dyn);
    fixed = new[4];
    q = new[2];
    assoc = new[3];
    scalar = new[1];
  end
endmodule
