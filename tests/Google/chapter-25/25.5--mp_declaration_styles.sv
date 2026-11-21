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
:name: mp_declaration_styles
:description: different ways of writing a modport declaration
:tags: 25.5
*/

// Checks in this file:
//   CHK1: one modport per modport statement
//   CHK2: two modports in a single modport statement separated by comma
//   CHK3: several signals sharing one direction: input a, b
//   CHK4: direction keyword repeated per signal: input a, input b
//   CHK5: modport list split over several lines

interface bus_if;
  logic a, b, c, d;

  // CHK1
  modport mp_one (input a, output b);

  // CHK2
  modport mp_two_a (input a), mp_two_b (output a);

  // CHK3
  modport mp_shared (input a, b, output c, d);

  // CHK4
  modport mp_repeat (input a, input b, output c, output d);

  // CHK5
  modport mp_multi_line (
    input  a,
           b,
    output c,
           d
  );
endinterface

module top;
  bus_if u_if();
endmodule
