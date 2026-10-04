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

// Tests for GenModHierPath.hlc (tests/GenModHierPath/dut.sv):
//
//   module ForNarrowRequest();
//     int array[10];
//   endmodule
//
//   module InitializedBlockRAM();
//     generate
//       if (1) begin : body
//           ForNarrowRequest ram ();
//       end
//     endgenerate
//
//     function automatic void InitializeMemory();
//       $readmemh(body.ram.array);
//     endfunction
//   endmodule // InitializedBlockRAM
//
// Compiled at "-d ast" level (no "-d inst"), so the generate-if survives as
// a GenIf on the module's getGenStmts() (IEEE 1800-2023 Sec 27.5) rather
// than being collapsed into an elaborated GenScopeArray/GenScope pair; the
// named body 'begin : body ... end' is a GenScope, and the module instance
//'ram' declared inside it is carried on GenScope::getModules() (a field
// GenScope shares with elaborated instances, Sec 27.3, 33.4 "Direct nesting
// of modules").
//
// What is under test: a *hierarchical path* reference into a module
// instantiated inside a generate block -- 'body.ram.array' (IEEE 1800-2023
// Sec 23.6 "Hierarchical names": a hierarchical name traverses named scopes
// -- here the generate block 'body', then the module instance 'ram' -- down
// to the item 'array'). Per IEEE 1800-2023 Sec 6.8 "Variable declarations":
// 'int array[10];' has no net-type keyword, so 'array' must be modeled as a
// Variable, never a Net, regardless of any '`default_nettype`.
//
// No .log file was consulted; accessor names were confirmed against the
// real hldb headers under
// E:\Davenche\davtests\davtests_02\build\include\hldb.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/gen_if.h>
#include <hldb/gen_scope.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/sys_func_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class GenModHierPathTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "GenModHierPath.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::GenIf *findGenIf(const hldb::Module *m) {
    if (m == nullptr || m->getGenStmts() == nullptr) return nullptr;
    for (const hldb::Any *const stmt : *m->getGenStmts()) {
      if (const hldb::GenIf *const gi = any_cast<hldb::GenIf>(stmt)) return gi;
    }
    return nullptr;
  }

  static const hldb::GenScope *getBody() {
    const hldb::GenIf *const gi = findGenIf(getModule("InitializedBlockRAM"));
    return (gi == nullptr) ? nullptr : gi->getStmt<hldb::GenScope>();
  }
};

TEST_F(GenModHierPathTest, BothModulesExist) {
  ASSERT_NE(getModule("ForNarrowRequest"), nullptr);
  ASSERT_NE(getModule("InitializedBlockRAM"), nullptr);
}

// 'int array[10];' -- no net-type keyword, so 'array' is a Variable
// (Sec 6.8), never a Net, regardless of implementation shortcuts.
TEST_F(GenModHierPathTest, ForNarrowRequestArrayIsVariableNotNet) {
  const hldb::Module *const m = getModule("ForNarrowRequest");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getVariables(), nullptr) << "'ForNarrowRequest' should declare 'array' as a variable";
  const hldb::Variable *const arr = hldb::findByName<hldb::Variable>("array", m->getVariables());
  EXPECT_NE(arr, nullptr) << "IEEE 1800-2023 Sec 6.8: 'int array[10]' has no net-type keyword and "
                              "must be modeled as a Variable";
  EXPECT_TRUE(m->getNets() == nullptr || hldb::findByName<hldb::Net>("array", m->getNets()) == nullptr)
      << "'array' must not additionally appear as a Net (Sec 6.7/6.8)";
}

// 'if (1) begin : body ... end' -- a single unlabeled-condition,
// named-block generate-if (Sec 27.5).
TEST_F(GenModHierPathTest, InitializedBlockRAMHasExactlyOneGenIfNamedBody) {
  const hldb::Module *const m = getModule("InitializedBlockRAM");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getGenStmts(), nullptr);
  size_t count = 0u;
  for (const hldb::Any *const stmt : *m->getGenStmts()) {
    if (any_cast<hldb::GenIf>(stmt) != nullptr) ++count;
  }
  EXPECT_EQ(count, 1u);

  const hldb::GenScope *const body = getBody();
  ASSERT_NE(body, nullptr) << "'begin : body ... end' should be a GenScope";
  EXPECT_EQ(body->getName(), std::string_view("body"));
}

// 'ForNarrowRequest ram ();' declared inside 'body' -- must be reachable
// through 'body's own module list (Sec 27.3, 33.4).
TEST_F(GenModHierPathTest, RamInstanceDeclaredInsideBody) {
  const hldb::GenScope *const body = getBody();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getModules(), nullptr) << "'body' should carry the 'ram' module instance";
  const hldb::Module *const ram = hldb::findByName<hldb::Module>("ram", body->getModules());
  ASSERT_NE(ram, nullptr) << "'ram' instance not found inside 'body'";
  EXPECT_EQ(ram->getDefName(), std::string_view("ForNarrowRequest"));
}

// 'function automatic void InitializeMemory();' -- declared directly on the
// module (not inside the generate block).
TEST_F(GenModHierPathTest, InitializeMemoryFunctionExists) {
  const hldb::Module *const m = getModule("InitializedBlockRAM");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getTaskFuncs(), nullptr);
  const hldb::Function *const fn = hldb::findByName<hldb::Function>("InitializeMemory", m->getTaskFuncs());
  ASSERT_NE(fn, nullptr);
  EXPECT_TRUE(fn->getAutomatic()) << "'function automatic' must set the automatic flag (Sec 13.4.2)";
}

// '$readmemh(body.ram.array);' -- the hierarchical path argument must
// resolve, through 'body' and 'ram', back to the Variable 'array' declared
// in 'ForNarrowRequest' (Sec 23.6).
TEST_F(GenModHierPathTest, ReadmemhArgumentIsHierPathToArray) {
  const hldb::Module *const m = getModule("InitializedBlockRAM");
  ASSERT_NE(m, nullptr);
  const hldb::Function *const fn = hldb::findByName<hldb::Function>("InitializeMemory", m->getTaskFuncs());
  ASSERT_NE(fn, nullptr);

  const hldb::SysFuncCall *const call = fn->getStmt<hldb::SysFuncCall>();
  ASSERT_NE(fn->getStmt(), nullptr) << "'$readmemh(...)' should be the function's sole statement";
  ASSERT_NE(call, nullptr) << "'$readmemh' should be a SysFuncCall";
  EXPECT_EQ(call->getName(), std::string_view("$readmemh"));

  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>((*call->getArguments())[0]);
  ASSERT_NE(arg, nullptr) << "'body.ram.array' should be a RefObj hierarchical reference";
  EXPECT_EQ(arg->getName(), std::string_view("body.ram.array"));

  ASSERT_NE(arg->getPathElems(), nullptr) << "'body.ram.array' should carry its dotted path elements";
  ASSERT_EQ(arg->getPathElems()->size(), 3u) << "path should be 'body' -> 'ram' -> 'array'";
  const hldb::Any *const first = arg->getPathElems()->at(0);
  const hldb::Any *const second = arg->getPathElems()->at(1);
  const hldb::Any *const last = arg->getPathElems()->at(2);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  ASSERT_NE(last, nullptr);
  EXPECT_EQ(first->getName(), std::string_view("body"));
  EXPECT_EQ(second->getName(), std::string_view("ram"));
  EXPECT_EQ(last->getName(), std::string_view("array"));

  // The reference must resolve back to the Variable declared in
  // 'ForNarrowRequest', reached via the 'ram' instance in 'body'.
  EXPECT_NE(arg->getActual(), nullptr);
  const hldb::Variable *const actual = arg->getActual<hldb::Variable>();
  ASSERT_NE(actual, nullptr) << "'body.ram.array' should resolve to the 'array' Variable";
  EXPECT_EQ(actual->getName(), std::string_view("array"));
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
