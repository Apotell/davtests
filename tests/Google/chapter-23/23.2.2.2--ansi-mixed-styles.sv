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

// 23.2.2.2: "Each module shall be declared either entirely with the
// list_of_ports syntax as described in 23.2.2.1 or entirely with the
// list_of_port_declarations syntax". Port a is non-ANSI, port b is ANSI.
module m(a, input b);
  input a;
endmodule
