/*
Copyright 2019 Alain Dargelas

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
 * File:   Compiler.h
 * Author: alain
 *
 * Created on March 4, 2017, 5:16 PM
 */

#ifndef SURELOG_COMPILEACTION_H
#define SURELOG_COMPILEACTION_H
#pragma once

namespace SURELOG {
enum class CompileAction { Preprocess, PostPreprocess, Parse };
}  // namespace SURELOG

#endif  // SURELOG_COMPILEACTION_H
