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

/*
:name: if_virtual_parameterized
:description: virtual interfaces of parameterized interfaces
:tags: 25.9
*/

// Checks in this file:
//   CHK1: virtual interface with explicit matching parameter #(16)
//   CHK2: virtual interface with default parameters assigned a default instance
//   CHK3: virtual interface with named parameter #(.W(4))
//   CHK4: virtual interface passed as a task argument

interface w_if #(parameter int W = 8);
  logic [W-1:0] data;
endinterface

module top;
  w_if #(16) u16();
  w_if u8();
  w_if #(.W(4)) u4();
  // CHK1
  virtual w_if #(16) v16;
  // CHK2
  virtual w_if v8;
  // CHK3
  virtual w_if #(.W(4)) v4;
  // CHK4
  task automatic show(virtual w_if #(16) v);
    $display("%h", v.data);
  endtask
  initial begin
    v16 = u16;
    v8 = u8;
    v4 = u4;
    show(v16);
  end
endmodule
