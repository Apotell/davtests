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
:name: if_dot_name_connection
:description: an interface port connected with the implicit .name form
:tags: 25.3.2
*/

interface bus_if;
  logic req;
endinterface

module sink(bus_if b);
endmodule

module top;
  bus_if b();
  sink u_sink(.b);
endmodule
