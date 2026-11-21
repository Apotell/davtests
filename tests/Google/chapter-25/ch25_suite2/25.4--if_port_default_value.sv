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
:name: if_port_default_value
:description: an interface input port with a default value left out at the instance
:tags: 25.4
*/

interface en_if(input logic clk, input logic en = 1'b1);
endinterface

module top;
  logic clk;
  en_if u_if(.clk(clk));
endmodule
