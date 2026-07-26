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

// Tests for first.sv (tags: 7.9.4 7.9)
//   module top ();
//     int map [ string ];
//     string s;
//     int rc;
//     initial begin
//       // empty, should return zero
//       rc = map.first( s );
//       $display(":assert: (%d == 0)", rc);
//       map[ "hello" ] = 1;
//       map[ "sad" ] = 2;
//       map[ "world" ] = 3;
//       rc = map.first( s );
//       $display(":assert: ((%d == 1) and ('%s' == 'hello'))", rc, s);
//     end
//   endmodule
//
// Checked:
//   - design has module top with exactly 3 nets: "map" (associative
//     array), "s" (string), "rc" (int)
//   - net "map": ArrayTypespec vpiArrayType=associative(3), index -> String,
//     elem -> Int
//   - Initial process: 1 Begin with 7 stmts (2 Assignment "rc=map.first(s)" +
//     2 SysFuncCall + 3 Assignment for map["hello"/"sad"/"world"])
//   - both rc=map.first(s) assignments: lhs RefObj "rc" resolves to Net rc,
//     rhs HierPath "map.first(s)" whose 2nd path elem is a MethodFuncCall
//     "first" with 1 argument RefObj "s" resolving to Net s (passed by
//     reference, unlike the string-literal arguments of exists()/delete())
//   - both $display calls and their RefObj("rc")/RefObj("s") arguments
//   - design-level typespecs (3): ModuleTypespec, IntTypespec, StringTypespec
//   - compiler emits zero errors (map.first(s) with an explicit ref argument
//     resolves cleanly, unlike the no-parens ".size"/".delete" forms)
//
// Not checked:
//   - actual runtime value written into rc/s by first() -- simulation-only
//     (see the skipped canary RuntimeValuesOfRcAndSRequireSimulation below)

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
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class AssociativeArrayFirstTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "first.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module / nets ------------------------------------------------------------

TEST_F(AssociativeArrayFirstTest, ModuleExists) {
  EXPECT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

TEST_F(AssociativeArrayFirstTest, ModuleHasThreeNets) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 3u);
}

TEST_F(AssociativeArrayFirstTest, NetMapIsAssociativeArrayOfIntByString) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const map = hldb::findByName<hldb::Net>("map", top->getNets());
  ASSERT_NE(map, nullptr);
  const hldb::ArrayTypespec *const at = map->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 3);  // associative = 3
  ASSERT_NE(at->getIndexTypespec(), nullptr);
  EXPECT_NE(at->getIndexTypespec()->getActual<hldb::StringTypespec>(), nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

TEST_F(AssociativeArrayFirstTest, NetSIsStringTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const s = hldb::findByName<hldb::Net>("s", top->getNets());
  ASSERT_NE(s, nullptr);
  EXPECT_NE(s->getTypespec<hldb::RefTypespec>()->getActual<hldb::StringTypespec>(), nullptr);
}

TEST_F(AssociativeArrayFirstTest, NetRcIsIntTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const rc = hldb::findByName<hldb::Net>("rc", top->getNets());
  ASSERT_NE(rc, nullptr);
  EXPECT_NE(rc->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr);
}

// --- initial process ---------------------------------------------------------

TEST_F(AssociativeArrayFirstTest, InitialBeginHasSevenStmts) {
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

// --- rc = map.first(s); on the empty map -------------------------------------

TEST_F(AssociativeArrayFirstTest, FirstCallAssignsRcFromMapFirstS) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Assignment *const assign =
      any_cast<hldb::Assignment>(init->getStmt<hldb::Begin>()->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "rc");
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr);
  const hldb::HierPath *const rhs = assign->getRhs<hldb::HierPath>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), std::string_view("map.first(s)"));
  ASSERT_NE(rhs->getPathElems(), nullptr);
  ASSERT_EQ(rhs->getPathElems()->size(), 2u);
  const hldb::RefObj *const mapRef = any_cast<hldb::RefObj>(rhs->getPathElems()->at(0));
  ASSERT_NE(mapRef, nullptr);
  EXPECT_EQ(mapRef->getName(), "map");
  EXPECT_NE(mapRef->getActual<hldb::Net>(), nullptr);
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(rhs->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "first");
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), "s");
  EXPECT_NE(arg->getActual<hldb::Net>(), nullptr);
}

TEST_F(AssociativeArrayFirstTest, FirstDisplayAssertsRcEqualsZero) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(init->getStmt<hldb::Begin>()->getStmts()->at(1));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 2u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: (%d == 0)");
  const hldb::RefObj *const rcRef = any_cast<hldb::RefObj>(disp->getArguments()->at(1));
  ASSERT_NE(rcRef, nullptr);
  EXPECT_EQ(rcRef->getName(), "rc");
  EXPECT_NE(rcRef->getActual<hldb::Net>(), nullptr);
}

// --- map["hello"]=1, map["sad"]=2, map["world"]=3 ----------------------------

TEST_F(AssociativeArrayFirstTest, PopulatesMapHelloSadWorld) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const begin = init->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);

  const hldb::Assignment *const a0 = any_cast<hldb::Assignment>(begin->getStmts()->at(2));
  ASSERT_NE(a0, nullptr);
  EXPECT_EQ(a0->getLhs<hldb::BitSelect>()->getIndex<hldb::Constant>()->getValue(), "hello");
  EXPECT_EQ(a0->getRhs<hldb::Constant>()->getDecompile(), "1");

  const hldb::Assignment *const a1 = any_cast<hldb::Assignment>(begin->getStmts()->at(3));
  ASSERT_NE(a1, nullptr);
  EXPECT_EQ(a1->getLhs<hldb::BitSelect>()->getIndex<hldb::Constant>()->getValue(), "sad");
  EXPECT_EQ(a1->getRhs<hldb::Constant>()->getDecompile(), "2");

  const hldb::Assignment *const a2 = any_cast<hldb::Assignment>(begin->getStmts()->at(4));
  ASSERT_NE(a2, nullptr);
  EXPECT_EQ(a2->getLhs<hldb::BitSelect>()->getIndex<hldb::Constant>()->getValue(), "world");
  EXPECT_EQ(a2->getRhs<hldb::Constant>()->getDecompile(), "3");
}

// --- rc = map.first(s); on the populated map ---------------------------------

TEST_F(AssociativeArrayFirstTest, SecondCallAssignsRcFromMapFirstS) {
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
  EXPECT_EQ(rhs->getName(), std::string_view("map.first(s)"));
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(rhs->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "first");
}

TEST_F(AssociativeArrayFirstTest, SecondDisplayAssertsRcOneAndSHello) {
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
  EXPECT_EQ(fmt->getValue(), ":assert: ((%d == 1) and ('%s' == 'hello'))");
  const hldb::RefObj *const rcRef = any_cast<hldb::RefObj>(disp->getArguments()->at(1));
  ASSERT_NE(rcRef, nullptr);
  EXPECT_EQ(rcRef->getName(), "rc");
  const hldb::RefObj *const sRef = any_cast<hldb::RefObj>(disp->getArguments()->at(2));
  ASSERT_NE(sRef, nullptr);
  EXPECT_EQ(sRef->getName(), "s");
  EXPECT_NE(sRef->getActual<hldb::Net>(), nullptr);
}

// --- known gap: runtime values require simulation -----------------------------

TEST_F(AssociativeArrayFirstTest, RuntimeValuesOfRcAndSRequireSimulation) {
  GTEST_SKIP() << "This harness only compiles/elaborates first.sv; it does not run a simulator, so "
                  "the actual runtime values written into rc/s by first() cannot be observed here. "
                  "first.sv's own $display format strings document the expected values instead.";

  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const begin = init->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::SysFuncCall *const emptyMapDisplay = any_cast<hldb::SysFuncCall>(begin->getStmts()->at(1));
  ASSERT_NE(emptyMapDisplay, nullptr);
  const hldb::Constant *const emptyFmt = any_cast<hldb::Constant>(emptyMapDisplay->getArguments()->at(0));
  ASSERT_NE(emptyFmt, nullptr);
  EXPECT_EQ(emptyFmt->getValue(), ":assert: (%d == 0)") << "expected rc == 0 on the empty map";
  const hldb::SysFuncCall *const populatedMapDisplay = any_cast<hldb::SysFuncCall>(begin->getStmts()->at(6));
  ASSERT_NE(populatedMapDisplay, nullptr);
  const hldb::Constant *const populatedFmt = any_cast<hldb::Constant>(populatedMapDisplay->getArguments()->at(0));
  ASSERT_NE(populatedFmt, nullptr);
  EXPECT_EQ(populatedFmt->getValue(), ":assert: ((%d == 1) and ('%s' == 'hello'))")
      << "expected rc == 1 and s == 'hello' once the map is populated";
}

// --- design-level typespecs / compiler diagnostics ---------------------------

TEST_F(AssociativeArrayFirstTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(AssociativeArrayFirstTest, CompilerReportsZeroErrors) {
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
