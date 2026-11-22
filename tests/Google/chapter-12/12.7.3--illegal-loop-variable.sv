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

// Illegal foreach loop variables (IEEE 1800-2023 Sec 12.7.3, 18.5.7.1).

class C;
  real A[4];
  constraint c1 { foreach (A[i, j]) A[i] > 0; }
  constraint c2 { foreach (A[A]) A[0] > 0; }
  constraint c3 { foreach (A[k]) A[k] > 0; }
endclass
module top;
  real a [2][3];
  real b [4];
  initial begin
    foreach (a[i, j, k]) begin end
    foreach (b[b]) begin end
    foreach (a[x, y]) begin end
    foreach (a[, y2]) begin end
  end
endmodule
