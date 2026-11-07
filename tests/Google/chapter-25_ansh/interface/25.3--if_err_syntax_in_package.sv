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
:name: if_err_syntax_in_package
:description: interface declared inside a package
:should_fail_because: an interface declaration is not a legal package item
:tags: 25.3
*/

// Errors in this file:
//   ERR1: interface declaration as a package item
// Syntax error: kept alone in its own file so it does not hide other checks.

package bus_pkg;
  interface bus_if;
    logic a;
  endinterface
endpackage

module top;
endmodule
