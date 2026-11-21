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
:name: if_generic_two_types
:description: one generic interface port connected to instances of two different interfaces
:tags: 25.3.3
*/

interface a_if;
  logic x;
endinterface

interface b_if;
  logic y;
endinterface

module sink(interface p);
endmodule

module top;
  a_if ua();
  b_if ub();
  sink s1(.p(ua));
  sink s2(.p(ub));
endmodule
