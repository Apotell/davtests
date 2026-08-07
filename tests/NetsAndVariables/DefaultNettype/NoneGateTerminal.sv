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
// net creation is disabled. Primitive gate terminals rely on implicit net
// creation just like continuous assignments and port connections do, so an
// undeclared identifier used as a gate terminal has no default net type to
// fall back on.

`default_nettype none

module illegal_construct_default_nettype_none_gate_terminal_test(input logic a, input logic b);
  // ILLEGAL: 'undeclared_gate_net' has no default net type to fall back on
  and (undeclared_gate_net, a, b);
endmodule

`default_nettype wire
