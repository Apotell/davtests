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

// Classes and interface classes extending or implementing the wrong kind of type (IEEE 1800-2023 Sec 8.26).

interface class IA;
  pure virtual function void f();
endclass
interface class IB extends IA;
endclass
class Base;
endclass
class Good extends Base implements IA;
  virtual function void f(); endfunction
endclass
class BadExtends extends IA;
endclass
interface class BadIfc extends Base;
endclass
class BadImpl implements Base;
endclass
class P #(type T = IA) implements T;
endclass
interface class Q #(type T = IA) extends T;
endclass
class R #(type T = Base) extends T;
endclass
