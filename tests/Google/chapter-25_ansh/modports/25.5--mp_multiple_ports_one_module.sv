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
:name: mp_multiple_ports_one_module
:description: one module with several modport ports and mixed ordinary ports
:tags: 25.5
*/

// Checks in this file:
//   CHK1: two ports of the same interface with different modports
//   CHK2: ports of two different interfaces with modports
//   CHK3: modport ports mixed with ordinary input/output ports
//   CHK4: data moved from one modport view to another inside the module
//   CHK5: connection by .* with matching instance names

interface bus_if;
  logic x, y;
  modport target (input x, output y);
  modport initiator (output x, input y);
endinterface

interface ctl_if;
  logic en;
  modport sink (input en);
endinterface

// CHK1 + CHK2 + CHK3
module bridge(input logic clk,
              bus_if.target up,
              bus_if.initiator down,
              ctl_if.sink ctl,
              output logic busy);
  // CHK4
  always_ff @(posedge clk) if (ctl.en) down.x <= up.x;
  assign up.y = down.y;
  assign busy = ctl.en;
endmodule

module top;
  logic clk, busy;
  bus_if up(), down();
  ctl_if ctl();
  // CHK5
  bridge u_br(.*);
endmodule
