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

// Tests for next.sv (tags: 7.9.6 7.9)
//   module top ();
//     int map [ string ];
//     string s;
//     int rc;
//     initial begin
//       map[ "hello" ] = 1;
//       map[ "sad" ] = 2;
//       map[ "world" ] = 3;
//       rc = map.first( s );
//       $display(":assert: ((%d == 1) and ('%s' == 'hello'))", rc, s);
//       rc = map.next( s );
//       $display(":assert: ((%d == 1) and ('%s' == 'sad'))", rc, s);
//     end
//   endmodule
//
// Checked:
//   - design has module top with exactly 3 variables: "map" (associative
//     array), "s" (string), "rc" (int)
//   - variable "map": ArrayTypespec vpiArrayType=associative(3), index -> String,
//     elem -> Int
//   - Initial process: 1 Begin with 7 stmts (3 Assignment for
//     map["hello"/"sad"/"world"] + Assignment rc=map.first(s) + SysFuncCall +
//     Assignment rc=map.next(s) + SysFuncCall)
//   - rc=map.first(s) assignment: lhs RefObj "rc" resolves to Variable rc, rhs
//     HierPath "map.first(s)" whose 2nd path elem is MethodFuncCall "first"
//   - rc=map.next(s) assignment: rhs HierPath "map.next(s)" whose 2nd path
//     elem is a MethodFuncCall "next" with 1 argument RefObj "s" resolving to
//     Variable s
//   - both $display calls and their RefObj("rc")/RefObj("s") arguments
//   - design-level typespecs (3): ModuleTypespec, IntTypespec, StringTypespec
//   - compiler emits zero errors (map.next(s) with an explicit ref argument
//     resolves cleanly, unlike the no-parens ".size"/".delete" forms)
//
// Not checked:
//   - actual runtime value written into rc/s by first()/next() --
//     simulation-only (see the skipped canary
//     RuntimeValuesOfRcAndSRequireSimulation below)

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/hier_path.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/variable.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class AssociativeArrayNextTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "next.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module / variables ----

TEST_F(AssociativeArrayNextTest, ModuleExists) {
  EXPECT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

TEST_F(AssociativeArrayNextTest, ModuleHasThreeVariables) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 3u);
}

TEST_F(AssociativeArrayNextTest, VariableMapIsAssociativeArrayOfIntByString) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const map = hldb::findByName<hldb::Variable>("map", top->getVariables());
  ASSERT_NE(map, nullptr);
  const hldb::ArrayTypespec *const at = map->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 3);  // associative = 3
  ASSERT_NE(at->getIndexTypespec(), nullptr);
  EXPECT_NE(at->getIndexTypespec()->getActual<hldb::StringTypespec>(), nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

// --- initial process ----

TEST_F(AssociativeArrayNextTest, InitialBeginHasSevenStmts) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const begin = init->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 7u);
}

// --- map["hello"]=1, map["sad"]=2, map["world"]=3 ----

TEST_F(AssociativeArrayNextTest, PopulatesMapHelloSadWorld) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const begin = init->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);

  const hldb::Assignment *const a0 = any_cast<hldb::Assignment>(begin->getStmts()->at(0));
  ASSERT_NE(a0, nullptr);
  EXPECT_EQ(a0->getLhs<hldb::BitSelect>()->getIndex<hldb::Constant>()->getValue(), "hello");
  EXPECT_EQ(a0->getRhs<hldb::Constant>()->getDecompile(), "1");

  const hldb::Assignment *const a1 = any_cast<hldb::Assignment>(begin->getStmts()->at(1));
  ASSERT_NE(a1, nullptr);
  EXPECT_EQ(a1->getLhs<hldb::BitSelect>()->getIndex<hldb::Constant>()->getValue(), "sad");
  EXPECT_EQ(a1->getRhs<hldb::Constant>()->getDecompile(), "2");

  const hldb::Assignment *const a2 = any_cast<hldb::Assignment>(begin->getStmts()->at(2));
  ASSERT_NE(a2, nullptr);
  EXPECT_EQ(a2->getLhs<hldb::BitSelect>()->getIndex<hldb::Constant>()->getValue(), "world");
  EXPECT_EQ(a2->getRhs<hldb::Constant>()->getDecompile(), "3");
}

// --- rc = map.first(s); ----

TEST_F(AssociativeArrayNextTest, FirstCallAssignsRcFromMapFirstS) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Assignment *const assign =
      any_cast<hldb::Assignment>(init->getStmt<hldb::Begin>()->getStmts()->at(3));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), "rc");
  const hldb::HierPath *const rhs = assign->getRhs<hldb::HierPath>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), std::string_view("map.first(s)"));
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(rhs->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "first");
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), "s");
  EXPECT_NE(arg->getActual<hldb::Variable>(), nullptr);
}

TEST_F(AssociativeArrayNextTest, FirstDisplayAssertsRcOneAndSHello) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(init->getStmt<hldb::Begin>()->getStmts()->at(4));
  ASSERT_NE(disp, nullptr);
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 3u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: ((%d == 1) and ('%s' == 'hello'))");
  EXPECT_EQ(any_cast<hldb::RefObj>(disp->getArguments()->at(1))->getName(), "rc");
  EXPECT_EQ(any_cast<hldb::RefObj>(disp->getArguments()->at(2))->getName(), "s");
}

// --- rc = map.next(s); ----

TEST_F(AssociativeArrayNextTest, SecondCallAssignsRcFromMapNextS) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Assignment *const assign =
      any_cast<hldb::Assignment>(init->getStmt<hldb::Begin>()->getStmts()->at(5));
  ASSERT_NE(assign, nullptr);
  EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), "rc");
  const hldb::HierPath *const rhs = assign->getRhs<hldb::HierPath>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), std::string_view("map.next(s)"));
  ASSERT_NE(rhs->getPathElems(), nullptr);
  ASSERT_EQ(rhs->getPathElems()->size(), 2u);
  const hldb::RefObj *const mapRef = any_cast<hldb::RefObj>(rhs->getPathElems()->at(0));
  ASSERT_NE(mapRef, nullptr);
  EXPECT_EQ(mapRef->getName(), "map");
  EXPECT_NE(mapRef->getActual<hldb::Variable>(), nullptr);
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(rhs->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "next");
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), "s");
  EXPECT_NE(arg->getActual<hldb::Variable>(), nullptr);
}

TEST_F(AssociativeArrayNextTest, SecondDisplayAssertsRcOneAndSSad) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(init->getStmt<hldb::Begin>()->getStmts()->at(6));
  ASSERT_NE(disp, nullptr);
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 3u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: ((%d == 1) and ('%s' == 'sad'))");
  EXPECT_EQ(any_cast<hldb::RefObj>(disp->getArguments()->at(1))->getName(), "rc");
  EXPECT_EQ(any_cast<hldb::RefObj>(disp->getArguments()->at(2))->getName(), "s");
}

// --- known gap: runtime values require simulation ----

TEST_F(AssociativeArrayNextTest, RuntimeValuesOfRcAndSRequireSimulation) {
  GTEST_SKIP() << "This harness only compiles/elaborates next.sv; it does not run a simulator, so "
                  "the actual runtime values written into rc/s by first()/next() cannot be observed "
                  "here. next.sv's own $display format strings document the expected values instead.";

  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const begin = init->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::SysFuncCall *const afterFirstDisplay = any_cast<hldb::SysFuncCall>(begin->getStmts()->at(4));
  ASSERT_NE(afterFirstDisplay, nullptr);
  const hldb::Constant *const firstFmt = any_cast<hldb::Constant>(afterFirstDisplay->getArguments()->at(0));
  ASSERT_NE(firstFmt, nullptr);
  EXPECT_EQ(firstFmt->getValue(), ":assert: ((%d == 1) and ('%s' == 'hello'))")
      << "expected rc == 1 and s == 'hello' after first()";
  const hldb::SysFuncCall *const afterNextDisplay = any_cast<hldb::SysFuncCall>(begin->getStmts()->at(6));
  ASSERT_NE(afterNextDisplay, nullptr);
  const hldb::Constant *const nextFmt = any_cast<hldb::Constant>(afterNextDisplay->getArguments()->at(0));
  ASSERT_NE(nextFmt, nullptr);
  EXPECT_EQ(nextFmt->getValue(), ":assert: ((%d == 1) and ('%s' == 'sad'))")
      << "expected rc == 1 and s == 'sad' after next()";
}

// --- design-level typespecs / compiler diagnostics ----

TEST_F(AssociativeArrayNextTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(AssociativeArrayNextTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
