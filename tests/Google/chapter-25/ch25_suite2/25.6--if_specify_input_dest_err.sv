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
:name: if_specify_input_dest_err
:description: a modport input signal is used as the destination of a module path
:should_fail_because: a module path destination shall be an output or inout terminal; the modport restricts d to input
:tags: 25.6
*/

interface itf;
  logic c, d;
  modport mp (input c, d);
endinterface

module m (itf.mp ch);
  specify
    (ch.c => ch.d) = 3;
  endspecify
endmodule
