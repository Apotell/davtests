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

// Illegal construct: SystemVerilog keywords are case-sensitive. Capitalizing
// a net type keyword makes it an unrecognized token, not the keyword
// itself, so `default_nettype Wire is not the same as `default_nettype wire.

`default_nettype Wire

module illegal_construct_default_nettype_wrong_case_test(input logic a, input logic b, output logic y);
  assign y = a ^ b;
endmodule

`default_nettype wire
