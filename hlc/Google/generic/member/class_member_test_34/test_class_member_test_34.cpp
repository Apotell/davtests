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

// Source under test: tests/Google/generic/member/class_member_test_34.sv
//
//   class myclass;
//   virtual function integer subroutine;
//     input a;
//     subroutine = a+42;
//   endfunction
//   endclass
//
// Checked (IEEE 1800-2023):
//   - Sec 8.3: class "myclass" exists, not virtual, with no extends, and
//     defines exactly one method.
//   - Sec 8.20 / 8.6: "subroutine" is a virtual, non-pure, public method
//     Function with automatic lifetime, returning integer.
//   - Sec 13.4: "input a;" in the body is an old-style formal declaration:
//     exactly one formal "a", direction input. Sec 13.3: with no data type
//     given, the formal defaults to logic (a 1-bit scalar, no range).
//   - Sec 13.4.1: the body is the blocking assignment "subroutine = a+42",
//     which sets the return value through the function name. Its right
//     side is a binary addition of "a" (bound to the formal) and the
//     constant 42. The binding is accepted either on the IODecl itself or
//     on the function-scope Variable "a" that HLC creates as the formal's
//     storage for an old-style "input a;" (confirmed by running this test,
//     not from the .log).
//   - The file is legal: no fatal, syntax or error diagnostics.
//
// Not checked:
//   - What the "subroutine" assignment target binds to (the implicit return
//     variable's representation is not fixed by the LRM).
//   - Whether the single assignment is wrapped in an implicit begin (both
//     shapes are accepted).
//   - vpiFullName on any object (never asserted per the test writing guide).

#include <hlc/Tests/Test.h>

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/class_defn.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/integer_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/logic_typespec.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {
class ClassMemberTest34Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_member_test_34.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::ClassDefn *getMyClass() {
    return hldb::findByName<hldb::ClassDefn>("myclass", m_design->getAllClasses());
  }

  static const hldb::Function *getSubroutine() {
    const hldb::ClassDefn *const cls = getMyClass();
    if (cls == nullptr || cls->getMethods() == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Function>("subroutine", cls->getMethods());
  }

  static const hldb::IODecl *getFormalA() {
    const hldb::Function *const fn = getSubroutine();
    if (fn == nullptr || fn->getIODecls() == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::IODecl>("a", fn->getIODecls());
  }

  // The function's only statement, looking through an implicit begin.
  static const hldb::Assignment *getBodyAssignment() {
    const hldb::Function *const fn = getSubroutine();
    if (fn == nullptr || fn->getStmt() == nullptr) {
      return nullptr;
    }
    const hldb::Begin *const wrapper = fn->getStmt<hldb::Begin>();
    if (wrapper == nullptr) {
      return fn->getStmt<hldb::Assignment>();
    }
    if (wrapper->getStmts() == nullptr || wrapper->getStmts()->size() != 1) {
      return nullptr;
    }
    return any_cast<hldb::Assignment>(wrapper->getStmts()->at(0));
  }
};

TEST_F(ClassMemberTest34Test, MyclassExists) {
  const hldb::ClassDefn *const cls = getMyClass();
  ASSERT_NE(cls, nullptr) << "'class myclass;' should create a ClassDefn";
  EXPECT_FALSE(cls->getVirtual()) << "'myclass' is not declared virtual";
  EXPECT_EQ(cls->getExtends(), nullptr) << "'myclass' has no extends clause";
  ASSERT_NE(cls->getMethods(), nullptr);
  EXPECT_EQ(cls->getMethods()->size(), 1u) << "'myclass' defines exactly one method, 'subroutine'";
}

TEST_F(ClassMemberTest34Test, SubroutineIsVirtualIntegerMethod) {
  const hldb::Function *const fn = getSubroutine();
  ASSERT_NE(fn, nullptr) << "'virtual function integer subroutine;' should be a Function in myclass";
  EXPECT_TRUE(fn->getMethod()) << "'subroutine' is defined inside a class, so it is a method";
  EXPECT_TRUE(fn->getVirtual()) << "Sec 8.20: 'subroutine' is declared virtual";
  EXPECT_FALSE(fn->getPure()) << "'subroutine' has a body, so it is not pure";
  EXPECT_EQ(fn->getVisibility(), vpiPublicVis) << "no local/protected qualifier: visibility is public";
  EXPECT_TRUE(fn->getAutomatic()) << "Sec 8.6: class methods have automatic lifetime";
  ASSERT_NE(fn->getReturn(), nullptr) << "'function integer subroutine' should carry a return RefTypespec";
  EXPECT_NE(fn->getReturn()->getActual<hldb::IntegerTypespec>(), nullptr) << "'subroutine' returns integer";
}

TEST_F(ClassMemberTest34Test, OldStyleFormalAIsInput) {
  const hldb::Function *const fn = getSubroutine();
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->getIODecls(), nullptr) << "Sec 13.4: 'input a;' in the body declares a formal";
  EXPECT_EQ(fn->getIODecls()->size(), 1u) << "'subroutine' has exactly one formal";
  const hldb::IODecl *const a = getFormalA();
  ASSERT_NE(a, nullptr) << "the formal should be named 'a'";
  EXPECT_EQ(a->getDirection(), vpiInput) << "'input a;' declares an input";
}

TEST_F(ClassMemberTest34Test, FormalADefaultsToScalarLogic) {
  // Sec 13.3: a formal declared without a data type defaults to logic.
  const hldb::IODecl *const a = getFormalA();
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a->getTypespec(), nullptr) << "Sec 13.3: 'input a;' still has the implicit type logic";
  const hldb::LogicTypespec *const logicTs = a->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(logicTs, nullptr) << "Sec 13.3: the default formal type is logic";
  EXPECT_TRUE(logicTs->getRanges() == nullptr || logicTs->getRanges()->empty())
      << "'input a;' has no packed range, so it is a 1-bit scalar";
}

TEST_F(ClassMemberTest34Test, BodyAssignsReturnValueThroughFunctionName) {
  const hldb::Assignment *const assign = getBodyAssignment();
  ASSERT_NE(assign, nullptr) << "the body should be the single assignment 'subroutine = a+42;'";
  EXPECT_TRUE(assign->getBlocking()) << "Sec 10.4.1: '=' is a blocking procedural assignment";
  ASSERT_NE(assign->getLhs(), nullptr);
  EXPECT_EQ(assign->getLhs()->getName(), "subroutine")
      << "Sec 13.4.1: assigning to the function name sets the return value";
}

TEST_F(ClassMemberTest34Test, BodyRhsIsAPlus42) {
  const hldb::Assignment *const assign = getBodyAssignment();
  ASSERT_NE(assign, nullptr);
  const hldb::Operation *const add = assign->getRhs<hldb::Operation>();
  ASSERT_NE(add, nullptr) << "'a+42' should be an Operation";
  EXPECT_EQ(add->getOpType(), vpiAddOp) << "Sec 11.4.3: '+' is binary addition";
  ASSERT_NE(add->getOperands(), nullptr);
  ASSERT_EQ(add->getOperands()->size(), 2u);
  EXPECT_EQ(add->getOperands()->at(0)->getName(), "a") << "the left operand is 'a'";
  const hldb::Constant *const fortyTwo = any_cast<hldb::Constant>(add->getOperands()->at(1));
  ASSERT_NE(fortyTwo, nullptr) << "the right operand '42' is a constant";
  EXPECT_EQ(fortyTwo->getDecompile(), "42");
}

TEST_F(ClassMemberTest34Test, OperandABindsToFormal) {
  const hldb::Assignment *const assign = getBodyAssignment();
  ASSERT_NE(assign, nullptr);
  const hldb::Operation *const add = assign->getRhs<hldb::Operation>();
  ASSERT_NE(add, nullptr);
  ASSERT_NE(add->getOperands(), nullptr);
  ASSERT_FALSE(add->getOperands()->empty());
  const hldb::RefObj *const ref = any_cast<hldb::RefObj>(add->getOperands()->at(0));
  ASSERT_NE(ref, nullptr) << "'a' should be a reference";
  const hldb::IODecl *const a = getFormalA();
  ASSERT_NE(a, nullptr);
  ASSERT_NE(ref->getActual(), nullptr) << "'a' must resolve to a declaration";
  if (ref->getActual() == a || ref->getActual() == a->getExpr()) {
    return;
  }
  // The formal may instead be held as the function-scope Variable "a"
  // created for 'input a;'; that is the formal's storage, not a separate
  // local (the function declares no other variable).
  const hldb::Variable *const storage = ref->getActual<hldb::Variable>();
  ASSERT_NE(storage, nullptr) << "'a' in the body binds to the formal 'input a'";
  EXPECT_EQ(storage->getName(), "a");
  EXPECT_EQ(storage->getParent(), getSubroutine()) << "the formal's storage is owned by 'subroutine'";
  const hldb::Function *const fn = getSubroutine();
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->getVariables(), nullptr);
  EXPECT_EQ(fn->getVariables()->size(), 1u) << "'subroutine' declares no variable other than its formal 'a'";
}

TEST_F(ClassMemberTest34Test, CompilesWithNoErrors) {
  const hlc::EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0) << "the source is legal SystemVerilog";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
