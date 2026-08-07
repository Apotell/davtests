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

// Validates the UHDM graph for a module using the enum prev() method:
//   module top();
//     typedef enum {a, b, c, d} e;
//     initial begin
//       e val = b;      // NOTE: initializer is "b", not "a"
//       val = val.prev();
//     end
//   endmodule
//
// What to check and why (IEEE 1800-2023 6.19.5.4 "Prev()", p.123):
// "function enum prev(int unsigned N = 1); The prev() method returns
// the Nth previous enumeration value ... A wrap to the end of the
// enumeration occurs when the start ... is reached." "val = val.prev();"
// is exactly this legal method call (initial value "b", not "a", so
// there is a genuine previous element). No :should_fail_because: tag.
//
// Checked:
//   - design has module top
//   - module has TypedefTypespec "e" -> EnumTypespec with 4 consts (a, b, c, d)
//   - Initial -> Begin has 1 Variable "val" (TypedefTypespec, inline init RefObj "b" -> EnumConst)
//     (NOTE: initializer is "b" not "a", since prev() needs a non-first value)
//   - Begin has 1 blocking assignment: val = val.prev()
//   - assignment rhs is HierPath "val.prev()"
//   - HierPath pathElems[0] is RefObj "val", pathElems[1] is FuncCall "prev" (no args)
//   - HierPath receiver RefObj "val" resolves to the local Variable
//   - prev() FuncCall carries no static return typespec (enum type is only
//     resolved at simulation runtime)

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/design.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/func_call.h>
#include <hldb/hier_path.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/scope.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>

namespace hlc {

class EnumPrevTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.19.5.4--enum_prev.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(EnumPrevTest, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Module typespec -- TypedefTypespec "e" -> EnumTypespec with 4 consts
// ---------------------------------------------------------------------------
TEST_F(EnumPrevTest, TypedefEExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const td = hldb::findByName<hldb::TypedefTypespec>("e", top->getTypespecs());
  ASSERT_NE(td, nullptr);
  EXPECT_NE(td->getTypedefAlias()->getActual<hldb::EnumTypespec>(), nullptr);
}

TEST_F(EnumPrevTest, EnumHasFourConsts) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const td = hldb::findByName<hldb::TypedefTypespec>("e", top->getTypespecs());
  ASSERT_NE(td, nullptr);
  const hldb::EnumTypespec *const enumTs = td->getTypedefAlias()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(enumTs, nullptr);
  ASSERT_NE(enumTs->getEnumConsts(), nullptr);
  EXPECT_EQ(enumTs->getEnumConsts()->size(), 4u);
  EXPECT_EQ(enumTs->getEnumConsts()->at(0)->getName(), "a");
  EXPECT_EQ(enumTs->getEnumConsts()->at(1)->getName(), "b");
  EXPECT_EQ(enumTs->getEnumConsts()->at(2)->getName(), "c");
  EXPECT_EQ(enumTs->getEnumConsts()->at(3)->getName(), "d");
}

// ---------------------------------------------------------------------------
// Variable "val" -- declared with inline initializer EnumConst "b" (not "a")
// ---------------------------------------------------------------------------
TEST_F(EnumPrevTest, ValVariableDeclaredWithInitB) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getVariables(), nullptr);
  ASSERT_EQ(blk->getVariables()->size(), 1u);
  const hldb::Variable *const val = blk->getVariables()->at(0);
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getName(), "val");
  EXPECT_NE(val->getTypespec()->getActual<hldb::TypedefTypespec>(), nullptr);
  const hldb::RefObj *const initVal = val->getValue<hldb::RefObj>();
  ASSERT_NE(initVal, nullptr) << "val inline initializer should be RefObj -> EnumConst";
  EXPECT_EQ(initVal->getName(), "b") << "prev() test uses 'b' as the initial value (not 'a')";
  EXPECT_NE(initVal->getActual<hldb::EnumConst>(), nullptr);
}

// ---------------------------------------------------------------------------
// Assignment: val = val.prev() -- rhs is HierPath "val.prev()"
// ---------------------------------------------------------------------------
TEST_F(EnumPrevTest, AssignmentRhsIsHierPath) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 1u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), "val");
  const hldb::HierPath *const hp = assign->getRhs<hldb::HierPath>();
  ASSERT_NE(hp, nullptr) << "val.prev() rhs should be a HierPath";
  EXPECT_EQ(hp->getName(), "val.prev");
}

TEST_F(EnumPrevTest, HierPathReceiverAndFuncCall) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  const hldb::HierPath *const hp = assign->getRhs<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  ASSERT_NE(hp->getPathElems(), nullptr);
  ASSERT_EQ(hp->getPathElems()->size(), 2u);
  const hldb::RefObj *const receiver = any_cast<hldb::RefObj>(hp->getPathElems()->at(0));
  ASSERT_NE(receiver, nullptr);
  EXPECT_EQ(receiver->getName(), "val");
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "prev");
  EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty()) << "prev() takes no arguments";
}

TEST_F(EnumPrevTest, HierPathReceiverResolvesToVariable) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  const hldb::HierPath *const hp = assign->getRhs<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  const hldb::RefObj *const receiver = any_cast<hldb::RefObj>(hp->getPathElems()->at(0));
  ASSERT_NE(receiver, nullptr);
  EXPECT_NE(receiver->getActual<hldb::Variable>(), nullptr)
      << "receiver RefObj 'val' in val.prev() should resolve to the local Variable";
}

TEST_F(EnumPrevTest, PrevCallHasNoStaticReturnTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  const hldb::HierPath *const hp = assign->getRhs<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getTypespec(), nullptr)
      << "prev() return type is only known at simulation runtime; HLC does not attach a static typespec";
}

TEST_F(EnumPrevTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
