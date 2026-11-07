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
:name: if_err_module_port_connections
:description: illegal connections to module interface ports
:should_fail_because: an interface port shall always be connected to an interface instance of the identical type
:tags: 23.3.3.4
*/

// Errors in this file:
//   ERR1: interface port left unconnected
//   ERR2: interface port connected with empty .b()
//   ERR3: named interface port connected to a different interface type
//   ERR4: interface port connected to an ordinary logic signal

interface if_a;
  logic x;
endinterface

interface if_b;
  logic x;
endinterface

module sub(if_a b);
endmodule

module top;
  if_b ib();
  logic not_an_if;
  // ERR1
  sub u1();
  // ERR2
  sub u2(.b());
  // ERR3
  sub u3(.b(ib));
  // ERR4
  sub u4(.b(not_an_if));
endmodule
