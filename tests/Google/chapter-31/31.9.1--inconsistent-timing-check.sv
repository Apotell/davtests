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

// Mutually inconsistent negative timing check limits (IEEE 1800-2023 Sec 31.9.1).

module m (input CP, input D, input TI, input A, input B);
  reg notifier;
  specify
    $setuphold(posedge CP, D, -10, 20, notifier);
    $setuphold(posedge CP, TI, -30, -20, notifier);
    $setuphold(posedge CP, A, -10, 15, notifier);
    $recrem(posedge CP, A, 12, -11, notifier);
    $setuphold(posedge CP, B, 5, -3, notifier);
    $recrem(posedge CP, B, 4, 2, notifier);
  endspecify
endmodule
