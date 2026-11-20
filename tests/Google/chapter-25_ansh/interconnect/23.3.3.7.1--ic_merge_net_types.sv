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
:name: ic_merge_net_types
:description: interconnect net merged with the net type of the ports it connects to
:tags: 23.3.3.7.1
*/

// Checks in this file:
//   CHK1: LRM netlist example - interconnect merges with wire and wand into wand
//   CHK2: interconnect connected to a tri port
//   CHK3: interconnect connected to a wor port and a wire port
//   CHK4: interconnect connected to an output wire logic port
//   CHK5: interconnect connected to a tri1 pull port

module dut1(inout wire w);
  assign w = 1;
endmodule

module dut2(inout wand w);
  assign w = 0;
endmodule

module tri_port(inout tri t);
endmodule

module wor_port(inout wor w);
  assign w = 1'b0;
endmodule

module wire_port(input wire w);
endmodule

module drv(output wire logic out);
  assign out = 1'b1;
endmodule

module pull_port(input tri1 t);
endmodule

module netlist;
  // CHK1
  interconnect iwire;
  dut1 child1(iwire);
  dut2 child2(iwire);

  // CHK2
  interconnect it;
  tri_port u_tri(it);

  // CHK3
  interconnect iw;
  wor_port  u_wor(iw);
  wire_port u_wp(iw);

  // CHK4
  interconnect io;
  drv u_drv(io);

  // CHK5
  interconnect ip;
  pull_port u_pull(ip);
endmodule
