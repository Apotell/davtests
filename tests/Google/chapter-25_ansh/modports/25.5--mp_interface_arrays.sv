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
:name: mp_interface_arrays
:description: modports combined with arrays of interfaces
:tags: 25.5
*/

// Checks in this file:
//   CHK1: modport selected on one array element .b(arr[1].target)
//   CHK2: module port declared as an array of modport views bus.target b[2]
//   CHK3: array of module instances with modport header connected to an interface array
//   CHK4: generate loop connecting arr[i].initiator
//   CHK5: element of an array port accessed through the modport b[1].x

interface bus_if;
  logic x, y;
  modport target (input x, output y);
  modport initiator (output x, input y);
endinterface

module single(bus_if b);
endmodule

module pair(bus_if.target b[2]);
  // CHK5
  assign b[0].y = b[1].x;
endmodule

module each(bus_if.target b);
  assign b.y = b.x;
endmodule

module drv(bus_if b);
endmodule

module top;
  bus_if arr[4]();
  bus_if two[2]();
  // CHK1
  single u1(.b(arr[1].target));
  // CHK2
  pair u2(.b(two));
  // CHK3
  each u3[4](.b(arr));
  // CHK4
  for (genvar i = 0; i < 4; i++) begin : g
    drv u_drv(.b(arr[i].initiator));
  end
endmodule
