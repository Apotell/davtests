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

// Tests for FuncInModule.hlc (tests/FuncInModule/dut.sv):
//   module rggen_or_reducer #(
//     parameter int WIDTH = 2,
//     parameter int N     = 4
//   )( ... );
//     function automatic bit [N-1:0][15:0] get_sub_n_list(int n);
//       ...
//     endfunction
//     function automatic bit [N-1:0][15:0] get_offset_list(bit [N-1:0][15:0] sub_n_list);
//       ...
//     endfunction
//     function automatic int get_next_n(bit [N-1:0][15:0] sub_n_list);
//       ...
//     endfunction
//     localparam bit [N-1:0][15:0] SUB_N_LIST_P = get_sub_n_list(N);
//     ...
//   endmodule
//
// This exercises "function defined directly inside a module" (IEEE
// 1800-2023 13.4, module_or_generate_item -> function_declaration): the
// three functions here are declared as direct items of module
// rggen_or_reducer's scope, not inside a package (contrast: FuncDef2 tests
// package-scoped functions). Per 13.4/3.13, a function declared inside a
// module is a member of that module's scope and is only visible to that
// module (and, per 23.3, its nested generate/instance scopes) -- there is
// no package-level TaskFuncCollection to hold these.
//
// What is checked:
//   - module rggen_or_reducer exists and its own (module-scope, not any
//     package's) getTaskFuncs() holds exactly 3 functions: get_sub_n_list,
//     get_offset_list, get_next_n
//   - all 3 are automatic (explicit 'automatic' keyword, 13.4.2)
//   - "get_sub_n_list" has exactly 1 formal IODecl "n", direction vpiInput,
//     typespec IntTypespec ('int')
//   - "get_sub_n_list" and "get_offset_list" return a packed array
//     (ArrayTypespec, getPacked() == true) per 1800-2023 7.4.2 "Packed
//     arrays" (multi-dimensional packed type 'bit [N-1:0][15:0]')
//   - "get_next_n" returns int -> IntTypespec, and its single formal
//     IODecl "sub_n_list" also resolves to a packed ArrayTypespec
//
// What is NOT checked and why: the exact nested element/range shape of the
// two-dimensional packed array typespec (e.g. the [15:0] inner dimension)
// is not asserted -- only that it is packed -- to avoid over-specifying an
// area with no existing precedent test in this suite to cross-check
// against; no .log file was consulted to decide this file's shape.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/int_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/module.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FuncInModuleTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FuncInModule.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("rggen_or_reducer", m_design->getAllModules());
  }

  static const hldb::Function *findFunc(std::string_view name) {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getTaskFuncs() == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>(name, top->getTaskFuncs());
  }
};

TEST_F(FuncInModuleTest, ModuleExists) { ASSERT_NE(getTop(), nullptr); }

TEST_F(FuncInModuleTest, ModuleHasExactlyThreeLocalFunctions) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  EXPECT_EQ(top->getTaskFuncs()->size(), 3u);
  EXPECT_NE(findFunc("get_sub_n_list"), nullptr);
  EXPECT_NE(findFunc("get_offset_list"), nullptr);
  EXPECT_NE(findFunc("get_next_n"), nullptr);
}

TEST_F(FuncInModuleTest, AllThreeFunctionsAreAutomatic) {
  for (std::string_view name : {"get_sub_n_list", "get_offset_list", "get_next_n"}) {
    const hldb::Function *const f = findFunc(name);
    ASSERT_NE(f, nullptr) << name;
    EXPECT_TRUE(f->getAutomatic()) << name << ": 13.4.2 explicit 'automatic' keyword";
  }
}

TEST_F(FuncInModuleTest, GetSubNListHasOneIntInputAndReturnsPackedArray) {
  const hldb::Function *const f = findFunc("get_sub_n_list");
  ASSERT_NE(f, nullptr);
  ASSERT_NE(f->getIODecls(), nullptr);
  ASSERT_EQ(f->getIODecls()->size(), 1u);
  const hldb::IODecl *const n = f->getIODecls()->at(0);
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(n->getName(), std::string_view("n"));
  EXPECT_EQ(n->getDirection(), vpiInput);
  ASSERT_NE(n->getTypespec(), nullptr);
  EXPECT_NE(n->getTypespec()->getActual<hldb::IntTypespec>(), nullptr) << "'int n' should resolve to IntTypespec";

  ASSERT_NE(f->getReturn(), nullptr);
  const hldb::ArrayTypespec *const ret = f->getReturn()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(ret, nullptr) << "'bit [N-1:0][15:0]' return type should resolve to ArrayTypespec";
  EXPECT_TRUE(ret->getPacked()) << "7.4.2: 'bit [N-1:0][15:0]' is a packed array";
}

TEST_F(FuncInModuleTest, GetOffsetListReturnsPackedArray) {
  const hldb::Function *const f = findFunc("get_offset_list");
  ASSERT_NE(f, nullptr);
  ASSERT_NE(f->getReturn(), nullptr);
  const hldb::ArrayTypespec *const ret = f->getReturn()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(ret, nullptr);
  EXPECT_TRUE(ret->getPacked());
}

TEST_F(FuncInModuleTest, GetNextNReturnsIntAndTakesPackedArrayInput) {
  const hldb::Function *const f = findFunc("get_next_n");
  ASSERT_NE(f, nullptr);
  ASSERT_NE(f->getReturn(), nullptr);
  EXPECT_NE(f->getReturn()->getActual<hldb::IntTypespec>(), nullptr)
      << "'function automatic int get_next_n(...)' should resolve its return typespec to IntTypespec";

  ASSERT_NE(f->getIODecls(), nullptr);
  ASSERT_EQ(f->getIODecls()->size(), 1u);
  const hldb::IODecl *const subNList = f->getIODecls()->at(0);
  ASSERT_NE(subNList, nullptr);
  EXPECT_EQ(subNList->getName(), std::string_view("sub_n_list"));
  ASSERT_NE(subNList->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const argTs = subNList->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(argTs, nullptr) << "'bit [N-1:0][15:0] sub_n_list' should resolve to ArrayTypespec";
  EXPECT_TRUE(argTs->getPacked());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
