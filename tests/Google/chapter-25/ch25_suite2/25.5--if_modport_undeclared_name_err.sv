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
:name: if_modport_undeclared_name_err
:description: a modport lists names that the interface never declares
:should_fail_because: a modport declaration shall not implicitly declare new ports; its names shall be declared by the interface
:tags: 25.5
*/

interface illegal_i;
  // a, b, c, d not declared by this interface
  modport initiator (input a, b, output c, d);
  modport target (output a, b, input c, d);
endinterface : illegal_i
