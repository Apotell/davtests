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
:name: ic_err_syntax_int_port
:description: interconnect port declared with an explicit data type
:should_fail_because: an interconnect port shall have no data type
:tags: 23.2.2
*/

// Errors in this file:
//   ERR1: input interconnect int p - data type on an interconnect port
// Syntax error: kept alone in its own file so it does not hide other checks.

module m(input interconnect int p);
endmodule

module top;
  wire [31:0] w;
  m u(w);
endmodule
