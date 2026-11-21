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
:name: if_conn_through_generate_err
:description: an interface port is connected to an interface instance reached through a generate block
:should_fail_because: a hierarchical reference used as an interface port actual shall not resolve through a generate block
:tags: 25.3
*/

interface bus_if;
  logic s;
  modport mp (input s);
endinterface

module sink (bus_if.mp p);
endmodule

module top;
  if (1) begin : g
    bus_if u_if();
  end
  sink u_sink (.p(g.u_if));
endmodule
