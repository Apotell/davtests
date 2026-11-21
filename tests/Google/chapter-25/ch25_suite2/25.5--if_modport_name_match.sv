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
:name: if_modport_name_match
:description: the same modport is named in the module header and in the port connection
:tags: 25.5
*/

interface i2;
  wire a, b;
  modport initiator (input a, output b);
  modport target (output a, input b);
endinterface

module m (i2.initiator i);
endmodule

module top;
  i2 i();
  m u1(.i(i.initiator));
endmodule
