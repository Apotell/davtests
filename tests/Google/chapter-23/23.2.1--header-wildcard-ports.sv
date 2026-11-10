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

// 23.2.1, Syntax 23-1, third module_declaration alternative:
// "module_keyword [lifetime] module_identifier ( . * ) ;". The port list
// is the wildcard ".*"; the ports themselves are declared in the body, as
// with a non-ANSI header.
module m(.*);
  input a;
  output b;
endmodule
