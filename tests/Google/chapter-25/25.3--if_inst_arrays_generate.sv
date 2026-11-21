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
:name: if_inst_arrays_generate
:description: arrays of interface instances and instances inside generate blocks
:tags: 25.3
*/

// Checks in this file:
//   CHK1: interface instance array with [9:0] range
//   CHK2: interface instance array with C-style size [4]
//   CHK3: interface instantiated inside a generate for loop
//   CHK4: interface instantiated inside a generate if/else
//   CHK5: member read through an array element arr[2].v

interface bus_if;
  logic [1:0] v;
  logic x;
endinterface

module top #(parameter bit USE = 1);
  // CHK1
  bus_if scalar1(), vector[9:0]();
  assign vector[3].x = scalar1.x;
  // CHK2
  bus_if arr[4]();
  assign arr[0].x = arr[3].x;
  // CHK3
  for (genvar i = 0; i < 3; i++) begin : g
    bus_if u_if();
    assign u_if.x = 1'b1;
  end
  // CHK4
  if (USE) begin : g_on
    bus_if u_if();
  end else begin : g_off
    logic unused;
  end
  // CHK5
  logic [1:0] y;
  assign y = arr[2].v;
endmodule
