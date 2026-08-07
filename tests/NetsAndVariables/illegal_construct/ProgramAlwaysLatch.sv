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

// Illegal construct: an 'always_latch' procedure inside a program. Per IEEE
// 1800 clause 24.3, a program can only contain initial or final procedures
// -- always procedures of any kind are illegal there.

program illegal_construct_program_always_latch_test;
  logic en;
  logic prog_var;
  always_latch begin // ILLEGAL: always_latch procedure inside a program
    if (en) prog_var = 1'b1;
  end
endprogram
