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

// 23.3.2, Syntax 23-6: parameter_value_assignment ::=
// # ( [ list_of_parameter_value_assignments ] ), either ordered or named.
// 23.10.2.2: with an empty named assignment "the parameter retains its
// default value".
module m #(parameter int W = 1, parameter int D = 2) ();
endmodule

module top;
  m u_default ();
  m #(4, 8) u_ordered ();
  m #(.D(16)) u_named ();
  m #(.W(), .D(32)) u_named_empty ();
endmodule
