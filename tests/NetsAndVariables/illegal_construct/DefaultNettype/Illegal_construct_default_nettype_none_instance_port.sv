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

// Illegal construct: once `default_nettype none is in effect, all implicit
// net creation is disabled. An undeclared identifier used as an
// unconnected module instance port connection has no default net type to
// fall back on.

`default_nettype none

module illegal_construct_default_nettype_none_instance_port_helper(input logic a, input logic b, output logic y);
  assign y = a & b;
endmodule

module illegal_construct_default_nettype_none_instance_port_test(input logic a, input logic b, output logic y);
  // ILLEGAL: 'undeclared_net' has no default net type to fall back on
  illegal_construct_default_nettype_none_instance_port_helper u_inst(.a(a), .b(b), .y(undeclared_net));
endmodule

`default_nettype wire
