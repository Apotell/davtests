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

// 23.2.2.2: "Generic interface ports can only be declared using the ANSI
// style list_of_port_declarations syntax." Adapted from the cpuMod example.
// "An interface port cannot be left unconnected" (23.3.3.4), so top
// connects both ports to an interface instance.
interface bus;
endinterface

module cpuMod(interface d, interface j);
endmodule

module top;
  bus b();
  cpuMod u(.d(b), .j(b));
endmodule
