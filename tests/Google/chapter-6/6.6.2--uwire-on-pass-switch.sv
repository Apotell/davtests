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

// uwire nets on the bidirectional terminals of pass switches (IEEE 1800-2023 Sec 6.6.2).

module top;
  uwire u1;
  uwire [3:0] ub;
  wire w1, w2, c;
  uwire uc;
  tran t0 (u1, w1);
  tranif1 t1 (w1, w2, uc);
  rtranif0 t2 (w2, ub[1], c);
  buf b0 (u1, w1);
  tran t3 (w1, w2);
endmodule
