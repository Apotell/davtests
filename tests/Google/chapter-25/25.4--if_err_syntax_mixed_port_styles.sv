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
:name: if_err_syntax_mixed_port_styles
:description: ANSI port header combined with a port declaration in the body
:should_fail_because: an ANSI-style header cannot be combined with port declarations in the body
:tags: 25.4
*/

// Errors in this file:
//   ERR1: input logic b; in the body of an ANSI-header interface
// Syntax error: kept alone in its own file so it does not hide other checks.

interface bus_if(input logic a);
  input logic b;
endinterface

module top;
  logic a;
  bus_if u_if(a);
endmodule
