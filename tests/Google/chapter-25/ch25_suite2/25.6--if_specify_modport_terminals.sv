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
:name: if_specify_modport_terminals
:description: signals of a modport port are used as specify path and timing check terminals
:tags: 25.6
*/

interface itf;
  logic c, q, d;
  modport flop (input c, d, output q);
endinterface

module dtype (itf.flop ch);
  always_ff @(posedge ch.c) ch.q <= ch.d;
  specify
    (posedge ch.c => (ch.q +: ch.d)) = (5, 6);
    $setup(ch.d, posedge ch.c, 1);
  endspecify
endmodule
