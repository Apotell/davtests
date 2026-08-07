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

// Illegal construct: `default_nettype requires exactly one argument;
// omitting it entirely is a compile error.

`default_nettype

module illegal_construct_default_nettype_missing_argument_test(input logic a, input logic b, output logic y);
  assign y = a ^ b;
endmodule

`default_nettype wire
