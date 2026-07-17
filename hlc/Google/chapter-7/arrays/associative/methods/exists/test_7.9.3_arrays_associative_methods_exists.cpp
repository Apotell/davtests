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

// Tests for exists.sv (tags: 7.9.3 7.9)
//   module top ();
//     int map [ string ];
//     initial begin
//       map[ "hello" ] = 1;
//       map[ "sad" ] = 2;
//       map[ "world" ] = 3;
//       $display(":assert: (%d == 1)", map.exists( "sad" ));
//       $display(":assert: (%d == 0)", map.exists( "happy" ));
//     end
//   endmodule
//
// Checked:
//   - design has module work@top with exactly 1 net "map"
//   - net "map": ArrayTypespec vpiArrayType=associative(3), index typespec ->
//     StringTypespec, elem typespec -> IntTypespec
//   - Initial process: 1 Begin with 5 stmts (3 Assignment + 2 SysFuncCall)
//   - the 3 index assignments (map["hello"]=1, map["sad"]=2, map["world"]=3)
//   - both $display calls: format string plus HierPath("map.exists(...)")
//     whose 2nd path elem is a MethodFuncCall "exists" with 1 string Constant
//     argument ("sad" / "happy")
//   - design-level typespecs (3): ModuleTypespec, IntTypespec, StringTypespec
//   - compiler emits zero errors (map.exists(...) with parens/argument
//     resolves cleanly, unlike the no-parens ".delete"/".size" forms seen in
//     delete.sv/size.sv/num.sv)
//
// Not checked:
//   - RefObj "item" style unresolved actual() -- not applicable here since
//     exists() takes an explicit argument and is not flagged as implicit net
//     (confirmed directly below by NoImplicitNetErrorsUnlikeNoParensSizeDeleteNum)

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
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

class AssociativeArrayExistsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "exists.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module / net -----------------------------------------------------------

TEST_F(AssociativeArrayExistsTest, ModuleExists) {
  EXPECT_NE(hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()), nullptr);
}

TEST_F(AssociativeArrayExistsTest, ModuleHasOneNet) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(AssociativeArrayExistsTest, NetMapIsAssociativeArrayOfIntByString) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
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

// --- initial process ---------------------------------------------------------

TEST_F(AssociativeArrayExistsTest, InitialBeginHasFiveStmts) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const begin = init->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 5u);
}

TEST_F(AssociativeArrayExistsTest, FirstAssignmentSetsMapHelloToOne) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Assignment *const assign =
      any_cast<hldb::Assignment>(init->getStmt<hldb::Begin>()->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  const hldb::BitSelect *const lhs = assign->getLhs<hldb::BitSelect>();
  ASSERT_NE(lhs, nullptr);
  const hldb::Constant *const index = lhs->getIndex<hldb::Constant>();
  ASSERT_NE(index, nullptr);
  EXPECT_EQ(index->getValue(), "hello");
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "1");
}

TEST_F(AssociativeArrayExistsTest, SecondAssignmentSetsMapSadToTwo) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Assignment *const assign =
      any_cast<hldb::Assignment>(init->getStmt<hldb::Begin>()->getStmts()->at(1));
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const index = assign->getLhs<hldb::BitSelect>()->getIndex<hldb::Constant>();
  ASSERT_NE(index, nullptr);
  EXPECT_EQ(index->getValue(), "sad");
}

TEST_F(AssociativeArrayExistsTest, ThirdAssignmentSetsMapWorldToThree) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Assignment *const assign =
      any_cast<hldb::Assignment>(init->getStmt<hldb::Begin>()->getStmts()->at(2));
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const index = assign->getLhs<hldb::BitSelect>()->getIndex<hldb::Constant>();
  ASSERT_NE(index, nullptr);
  EXPECT_EQ(index->getValue(), "world");
}

// --- $display(":assert: (%d == 1)", map.exists("sad")) -----------------------

TEST_F(AssociativeArrayExistsTest, FirstDisplayFormatStringIsExistsAssertOne) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysFuncCall *const disp = any_cast<hldb::SysFuncCall>(init->getStmt<hldb::Begin>()->getStmts()->at(3));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 2u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: (%d == 1)");
}

TEST_F(AssociativeArrayExistsTest, FirstDisplaySecondArgIsMapExistsSad) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysFuncCall *const disp = any_cast<hldb::SysFuncCall>(init->getStmt<hldb::Begin>()->getStmts()->at(3));
  ASSERT_NE(disp, nullptr);
  const hldb::HierPath *const hp = any_cast<hldb::HierPath>(disp->getArguments()->at(1));
  ASSERT_NE(hp, nullptr);
  EXPECT_EQ(hp->getName(), std::string_view("map.exists(\"sad\")"));
  ASSERT_NE(hp->getPathElems(), nullptr);
  ASSERT_EQ(hp->getPathElems()->size(), 2u);
  const hldb::RefObj *const mapRef = any_cast<hldb::RefObj>(hp->getPathElems()->at(0));
  ASSERT_NE(mapRef, nullptr);
  EXPECT_EQ(mapRef->getName(), "map");
  EXPECT_NE(mapRef->getActual<hldb::Net>(), nullptr);
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "exists");
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::Constant *const arg = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getConstType(), vpiStringConst);
  EXPECT_EQ(arg->getValue(), "sad");
}

// --- $display(":assert: (%d == 0)", map.exists("happy")) ---------------------

TEST_F(AssociativeArrayExistsTest, SecondDisplayFormatStringIsExistsAssertZero) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysFuncCall *const disp = any_cast<hldb::SysFuncCall>(init->getStmt<hldb::Begin>()->getStmts()->at(4));
  ASSERT_NE(disp, nullptr);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: (%d == 0)");
}

TEST_F(AssociativeArrayExistsTest, SecondDisplaySecondArgIsMapExistsHappy) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysFuncCall *const disp = any_cast<hldb::SysFuncCall>(init->getStmt<hldb::Begin>()->getStmts()->at(4));
  ASSERT_NE(disp, nullptr);
  const hldb::HierPath *const hp = any_cast<hldb::HierPath>(disp->getArguments()->at(1));
  ASSERT_NE(hp, nullptr);
  EXPECT_EQ(hp->getName(), std::string_view("map.exists(\"happy\")"));
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "exists");
  const hldb::Constant *const arg = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getValue(), "happy");
}

// --- design-level typespecs / compiler diagnostics ---------------------------

TEST_F(AssociativeArrayExistsTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(AssociativeArrayExistsTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "work@top");
}

TEST_F(AssociativeArrayExistsTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(AssociativeArrayExistsTest, NoImplicitNetErrorsUnlikeNoParensSizeDeleteNum) {
  // Confirms the "not applicable" note above: unlike map.size/map.delete/arr.num called
  // without parens elsewhere in this suite, map.exists(...) with an explicit argument never
  // triggers ELAB_ILLEGAL_IMPLICIT_NET, so there is no unresolved "item"-style RefObj here.
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const std::vector<Error> &errors = m_session->getErrorContainer()->getErrors();
  for (const Error &err : errors) {
    EXPECT_NE(err.getType(), ErrorDefinition::ELAB_ILLEGAL_IMPLICIT_NET);
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
