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

// 23.2.2.4: "[ input ] [ type ] port_identifier = constant_expression". A
// default on an input port of a variable type; top omits the port, so "the
// compiler shall insert the corresponding default values".
module m(input var logic [7:0] d = 8'h5A);
endmodule

module top;
  m u();
endmodule
