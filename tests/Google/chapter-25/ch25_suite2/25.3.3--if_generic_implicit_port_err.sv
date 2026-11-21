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
:name: if_generic_implicit_port_err
:description: a generic interface port is connected only through the .* implicit form
:should_fail_because: an implicit port cannot be used to reference a generic interface; a named port shall be used
:tags: 25.3.3
*/

interface simple_bus;
  logic req;
endinterface

module memMod (interface sb_intf, input logic clk);
endmodule

module top;
  logic clk = 0;
  simple_bus sb_intf();
  memMod mem (.*);
endmodule
