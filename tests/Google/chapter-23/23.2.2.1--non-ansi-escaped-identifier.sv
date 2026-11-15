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

// 23.2.2.1: a port reference can be "A simple identifier or escaped
// identifier". 5.6.1: "The leading backslash character and the terminating
// white space are not considered to be part of an identifier."
module m(\bus+index , c);
  input \bus+index ;
  input c;
endmodule
