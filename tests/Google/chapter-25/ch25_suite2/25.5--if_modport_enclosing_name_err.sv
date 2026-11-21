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
:name: if_modport_enclosing_name_err
:description: a modport lists names declared by an enclosing interface
:should_fail_because: all names used in a modport shall be declared by the same interface as the modport itself
:tags: 25.5
*/

interface i;
  wire x, y;
  interface illegal_i;
    wire a, b, c, d;
    // x, y not declared by this interface
    modport initiator (input a, b, x, output c, d, y);
    modport target (output a, b, x, input c, d, y);
  endinterface : illegal_i
endinterface : i
