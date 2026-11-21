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
:name: if_wrong_bundle_type_err
:description: an interface port connected to an instance of a different interface
:should_fail_because: an interface port shall be connected to an interface instance of the identical type
:tags: 25.3.2
*/

interface a_if;
  logic s;
endinterface

interface b_if;
  logic s;
endinterface

module sink(a_if p);
endmodule

module top;
  b_if u_b();
  sink u_sink(.p(u_b));
endmodule
