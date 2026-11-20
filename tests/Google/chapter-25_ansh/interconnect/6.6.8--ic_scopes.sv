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
:name: ic_scopes
:description: interconnect nets declared in different scopes
:tags: 6.6.8
*/

// Checks in this file:
//   CHK1: interconnect declared inside a generate for loop
//   CHK2: interconnect declared inside a generate if block
//   CHK3: interconnect declared inside a nested module
//   CHK4: interconnect declared inside an interface and connected by hierarchical name
//   CHK5: interconnect port on an interface

module leaf(inout wire p);
endmodule

// CHK4
interface ic_if;
  interconnect inner;
endinterface

// CHK5
interface port_if(inout interconnect ext);
endinterface

module top #(parameter bit EN = 1);
  // CHK1
  for (genvar i = 0; i < 2; i++) begin : g
    interconnect n;
    leaf u(n);
  end
  // CHK2
  if (EN) begin : g_en
    interconnect n;
    leaf u(n);
  end
  // CHK3
  module inner_mod;
    interconnect n;
    leaf u(n);
  endmodule
  inner_mod u_inner();

  ic_if u_if();
  leaf u_hier(u_if.inner);

  wire w;
  port_if u_pif(w);
endmodule
