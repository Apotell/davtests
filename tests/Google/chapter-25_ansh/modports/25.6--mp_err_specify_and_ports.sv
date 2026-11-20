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
:name: mp_err_specify_and_ports
:description: illegal uses of modport signals as module terminals and ports
:should_fail_because: ref ports cannot be specify terminals and path directions and terminals must follow the modport
:tags: 25.6
*/

// Errors in this file:
//   ERR1: ref modport signal used as a specify path terminal
//   ERR2: ref modport signal used in a $setup timing check
//   ERR3: path from a modport output to a modport input (reversed direction)
//   ERR4: specify path to a signal not listed in the modport

interface itf;
  logic c, d, q, hidden;
  modport flop (input c, ref d, output q);
endinterface

// ERR1
module m1(itf.flop ch);
  specify
    (ch.d => ch.q) = 1;
  endspecify
endmodule

// ERR2
module m2(itf.flop ch);
  specify
    $setup(ch.d, posedge ch.c, 1);
  endspecify
endmodule

// ERR3
module m3(itf.flop ch);
  specify
    (ch.q => ch.c) = 1;
  endspecify
endmodule

// ERR4
module m4(itf.flop ch);
  specify
    (ch.c => ch.hidden) = 1;
  endspecify
endmodule

module top;
  itf u();
  m1 u1(u);
  m2 u2(u);
  m3 u3(u);
  m4 u4(u);
endmodule
