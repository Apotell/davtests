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

// Validates the UHDM graph for a module using the enum first() method:
//   module top();
//     typedef enum {a, b, c, d} e;
//     initial begin
//       e val = a;
//       val = val.first();
//     end
//   endmodule
//
// What to check and why (IEEE 1800-2023 6.19.5.1 "First()", p.122):
// "function enum first(); The first() method returns the value of the
// first member of the enumeration." "val = val.first();" is exactly
// this legal method call. No :should_fail_because: tag.
//
// Checked:
//   - design has module top
//   - module has TypedefTypespec "e" -> EnumTypespec with 4 consts (a, b, c, d)
//   - Initial -> Begin has 1 Variable "val" (TypedefTypespec, inline init RefObj "a" -> EnumConst)
//   - Begin has 1 blocking assignment: val = val.first()
//   - assignment rhs is RefObj "val.first()"
//   - RefObj pathElems[0] is RefObj "val", pathElems[1] is FuncCall "first" (no args)
//   - RefObj receiver RefObj "val" resolves to the local Variable
//   - first() FuncCall carries no static return typespec (enum type is only
//     resolved at simulation runtime)

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/class_defn.h>
#include <hldb/design.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/func_call.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/scope.h>
#include <hldb/task_func.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>

namespace hlc {

class EnumFirstTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.19.5.1--enum_first.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(EnumFirstTest, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Module typespec -- TypedefTypespec "e" -> EnumTypespec with 4 consts
// ---------------------------------------------------------------------------
TEST_F(EnumFirstTest, TypedefEExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const tt = hldb::findByName<hldb::TypedefTypespec>("e", top->getTypespecs());
  ASSERT_NE(tt, nullptr);
  const hldb::Typedef *const td = tt->getTypedef();
  ASSERT_NE(td, nullptr);
  EXPECT_NE(td->getAlias()->getActual<hldb::EnumTypespec>(), nullptr);
}

TEST_F(EnumFirstTest, EnumHasFourConsts) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const tt = hldb::findByName<hldb::TypedefTypespec>("e", top->getTypespecs());
  ASSERT_NE(tt, nullptr);
  const hldb::Typedef *const td = tt->getTypedef();
  ASSERT_NE(td, nullptr);
  const hldb::EnumTypespec *const enumTs = td->getAlias()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(enumTs, nullptr);
  const hldb::Enum *const e = enumTs->getEnum();
  ASSERT_NE(e, nullptr);
  ASSERT_NE(e->getEnumConsts(), nullptr);
  EXPECT_EQ(e->getEnumConsts()->size(), 4u);
  EXPECT_EQ(e->getEnumConsts()->at(0)->getName(), std::string_view("a"));
  EXPECT_EQ(e->getEnumConsts()->at(1)->getName(), std::string_view("b"));
  EXPECT_EQ(e->getEnumConsts()->at(2)->getName(), std::string_view("c"));
  EXPECT_EQ(e->getEnumConsts()->at(3)->getName(), std::string_view("d"));
}

// ---------------------------------------------------------------------------
// Variable "val" -- declared with inline initializer EnumConst "a"
// ---------------------------------------------------------------------------
TEST_F(EnumFirstTest, ValVariableDeclaredWithInitA) {
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
  EXPECT_EQ(val->getName(), std::string_view("val"));
  EXPECT_NE(val->getTypespec()->getActual<hldb::TypedefTypespec>(), nullptr);
  const hldb::RefObj *const initVal = val->getValue<hldb::RefObj>();
  ASSERT_NE(initVal, nullptr) << "val inline initializer should be RefObj -> EnumConst";
  EXPECT_EQ(initVal->getName(), std::string_view("a"));
  EXPECT_NE(initVal->getActual<hldb::EnumConst>(), nullptr);
}

// ---------------------------------------------------------------------------
// Assignment: val = val.first() -- rhs is RefObj "val.first()"
// ---------------------------------------------------------------------------
TEST_F(EnumFirstTest, AssignmentRhsIsRefObj) {
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
  EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), std::string_view("val"));
  const hldb::RefObj *const hp = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(hp, nullptr) << "val.first() rhs should be a RefObj";
  EXPECT_EQ(hp->getName(), std::string_view("val.first"));
}

TEST_F(EnumFirstTest, RefObjReceiverAndFuncCall) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const hp = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(hp, nullptr);
  ASSERT_NE(hp->getPathElems(), nullptr);
  ASSERT_EQ(hp->getPathElems()->size(), 2u);
  const hldb::RefObj *const receiver = any_cast<hldb::RefObj>(hp->getPathElems()->at(0));
  ASSERT_NE(receiver, nullptr);
  EXPECT_EQ(receiver->getName(), std::string_view("val"));
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), std::string_view("first"));
  EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty()) << "first() takes no arguments";
}

TEST_F(EnumFirstTest, RefObjReceiverResolvesToVariable) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const hp = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(hp, nullptr);
  const hldb::RefObj *const receiver = any_cast<hldb::RefObj>(hp->getPathElems()->at(0));
  ASSERT_NE(receiver, nullptr);
  EXPECT_NE(receiver->getActual<hldb::Variable>(), nullptr)
      << "receiver RefObj 'val' in val.first() should resolve to the local Variable";
}

TEST_F(EnumFirstTest, FirstCallBindsToBuiltinEnumTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const hp = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(hp, nullptr);
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  // IEEE 1800-2023 Sec 6.19.5.1: "first()" is an enumerated-type method. It is declared in
  // no user scope, so it can only resolve to the builtin "EnumTypespec" class.
  const hldb::TaskFunc *const tf = call->getTaskFunc();
  ASSERT_NE(tf, nullptr) << "enum.first() must bind (IEEE 1800-2023 Sec 6.19.5.1)";
  EXPECT_EQ(tf->getName(), "first");
  const hldb::ClassDefn *const owner = any_cast<hldb::ClassDefn>(tf->getParent());
  ASSERT_NE(owner, nullptr);
  EXPECT_EQ(owner->getName(), "EnumTypespec");
}

TEST_F(EnumFirstTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  // COMP_FAILED_TO_BIND does not fire for an unresolved enumerated-type method; the Linter
  // reports LINT_NULL_ACTUAL instead, so check that (see Sec 6.19.5.1).
  EXPECT_EQ(findError(ErrorDefinition::LINT_NULL_ACTUAL), nullptr)
      << "enum.first() must bind (IEEE 1800-2023 Sec 6.19.5.1)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
