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
:name: if_specify_ref_terminal_err
:description: a ref modport signal is used as a specify path terminal
:should_fail_because: a ref port cannot be used as a terminal in a specify block
:tags: 25.6
*/

interface itf;
  logic c, q, r;
  modport mp (input c, output q, ref r);
endinterface

module m (itf.mp ch);
  specify
    (ch.r => ch.q) = 3;
  endspecify
endmodule
