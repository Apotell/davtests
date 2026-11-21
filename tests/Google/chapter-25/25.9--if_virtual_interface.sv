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
:name: if_virtual_interface
:description: virtual interface declarations and uses
:tags: 25.9
*/

// Checks in this file:
//   CHK1: virtual interface variable assigned an instance and used
//   CHK2: declaration with the optional interface keyword
//   CHK3: virtual interface as a class member set in the constructor
//   CHK4: comparison with null before assignment
//   CHK5: array of virtual interfaces filled from an instance array

interface bus_if(input logic clk);
  logic a, req;
endinterface

// CHK3
class driver;
  virtual bus_if vif;
  function new(virtual bus_if v);
    vif = v;
  endfunction
  task run();
    @(posedge vif.clk) vif.req <= 1'b1;
  endtask
endclass

module top;
  logic clk;
  bus_if u_if(clk);
  bus_if arr[2](clk);
  // CHK1
  virtual bus_if v1;
  initial begin
    v1 = u_if;
    v1.a = 1'b1;
  end
  // CHK2
  virtual interface bus_if v2 = u_if;
  driver d;
  initial begin
    d = new(u_if);
    d.run();
  end
  // CHK4
  virtual bus_if v4;
  initial if (v4 == null) v4 = u_if;
  // CHK5
  virtual bus_if vifs[2];
  initial begin
    vifs[0] = arr[0];
    vifs[1] = arr[1];
  end
endmodule
