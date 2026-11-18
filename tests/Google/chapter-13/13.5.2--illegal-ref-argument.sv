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

// Nets and selects into nets passed by reference (IEEE 1800-2023 Sec 13.5.2).

module top;
  wire [7:0] n;
  logic [7:0] v;
  int arr[4];
  task automatic t(ref logic [7:0] a);
  endtask
  task automatic tb(ref logic b);
  endtask
  task automatic ti(ref int i);
  endtask
  initial begin
    t(n);
    tb(n[0]);
    t(v);
    tb(v[1]);
    ti(arr[2]);
  end
endmodule
