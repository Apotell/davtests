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

// Tests for dut.sv (tags: ForLoopBind). Note: despite the "Bind" in this
// test's name, dut.sv contains no bind_directive (IEEE 1800-2023 Sec
// 23.11) anywhere -- checked directly in the source below before writing
// any assertions, per this repo's instruction to read each dut.sv rather
// than assume from the test name. What it actually exercises is a
// procedural "for" loop (12.7.1) nested inside a "foreach" loop (12.7.3),
// inside a task defined out-of-block (Sec 8.24) via a class-scope
// qualifier:
//
//   package uvm;
//     class uvm_vreg_cb_iter; endclass
//     class uvm_vreg_field_cb_iter;
//       function int first(); endfunction
//       function int next(); endfunction
//     endclass
//     class uvm_vreg_field_cbs;
//       int fname;
//     endclass
//     class uvm_vreg_field; endclass
//     class uvm_vreg;
//        local uvm_vreg_field fields[$];
//        extern task write();
//     endclass
//     task uvm_vreg::write();
//        uvm_vreg_cb_iter cbs = new(this);
//        foreach (fields[i]) begin
//           uvm_vreg_field_cb_iter cbs = new(fields[i]);
//           for (uvm_vreg_field_cbs cb = cbs; cb != null;
//                cb = cbs) begin
//              cb.fname = 1;
//           end
//        end
//     endtask
//   endpackage
//
// What to check and why:
//   - Sec 8.24 "Out-of-block declarations": "extern task write();" inside
//     class 'uvm_vreg' is a matching extern prototype for the out-of-block
//     "task uvm_vreg::write();" -- per 8.24 the out-of-block body is the
//     task's definition and the resulting Task must be reachable from
//     class 'uvm_vreg's own scope (getTaskFuncs()), the same as an
//     in-body task declaration would be.
//   - Sec 6.20.4/local variable declarations with initializers ("...cbs =
//     new(this);", "...cbs = new(fields[i]);") are declarations, not
//     assignments: per this suite's ForeachClass/test_ForeachClass.cpp
//     precedent, they appear directly in the enclosing Begin's statement
//     list as Variable objects (not wrapped in an Assignment).
//   - Sec 12.7.3 "foreach": "foreach (fields[i])" -- array_name RefObj
//     "fields" (the class-local queue member), one implicit loop variable
//     "i".
//   - Sec 12.7.1 "for": "for (uvm_vreg_field_cbs cb = cbs; cb != null; cb
//     = cbs)" is a for_variable_declaration form -- "cb" must be declared
//     local to the ForStmt's own scope (getVariables()), not at the
//     foreach's or task's scope.
//   - the for-loop's condition "cb != null" must be an Operation with
//     vpiNeqOp comparing 2 operands.
//   - the innermost body "cb.fname = 1;" is an Assignment whose LHS is a
//     RefObj with flattened name "cb.fname" (dotted member-access path,
//     consistent with how this suite elsewhere flattens indexed/dotted
//     lvalues into a single RefObj, e.g.
//     DoWhile/DoWhile/test_DoWhile.cpp's "m_sync[i].m_state").
//
// What is NOT checked and why:
//   - the exact typespec modeling of the queue "fields[$]" (packed vs.
//     unpacked, QueueTypespec shape) -- out of scope for this file, which
//     is about the for/foreach nesting, not queue-type modeling
//   - whether "cb != null"'s RHS resolves to a dedicated null-constant
//     VPI type: IEEE 1800-2023 does not mandate one particular HLDB
//     representation for the 'null' keyword, so only the Operation shape
//     (vpiNeqOp, 2 operands, LHS is RefObj "cb") is asserted

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/class_defn.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/for_stmt.h>
#include <hldb/foreach_stmt.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/ref_obj.h>
#include <hldb/task.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ForLoopBindTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ForLoopBind.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getUvm() {
    return hldb::findByName<hldb::Package>("uvm", m_design->getAllPackages());
  }

  static const hldb::ClassDefn *getClass(std::string_view name) {
    const hldb::Package *const uvm = getUvm();
    if (uvm == nullptr || uvm->getClassDefns() == nullptr) return nullptr;
    return hldb::findByName<hldb::ClassDefn>(name, uvm->getClassDefns());
  }

  static const hldb::ClassDefn *getUvmVreg() { return getClass("uvm_vreg"); }

  // Sec 8.24: the out-of-block "task uvm_vreg::write();" must bind to the
  // matching "extern task write();" prototype and be reachable from
  // uvm_vreg's own scope.
  static const hldb::Task *getWrite() {
    const hldb::ClassDefn *const cls = getUvmVreg();
    if (cls == nullptr || cls->getTaskFuncs() == nullptr) return nullptr;
    return hldb::findByName<hldb::Task>("write", cls->getTaskFuncs());
  }

  static const hldb::Begin *getWriteBody() {
    const hldb::Task *const write = getWrite();
    if (write == nullptr) return nullptr;
    return write->getStmt<hldb::Begin>();
  }

  static const hldb::ForeachStmt *getForeachFields() {
    const hldb::Begin *const body = getWriteBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->size() < 2) return nullptr;
    return any_cast<hldb::ForeachStmt>(body->getStmts()->at(1));
  }

  static const hldb::Begin *getForeachBody() {
    const hldb::ForeachStmt *const fe = getForeachFields();
    if (fe == nullptr) return nullptr;
    return fe->getStmt<hldb::Begin>();
  }

  static const hldb::ForStmt *getForCb() {
    const hldb::Begin *const foreachBody = getForeachBody();
    if (foreachBody == nullptr || foreachBody->getStmts() == nullptr || foreachBody->getStmts()->size() < 2)
      return nullptr;
    return any_cast<hldb::ForStmt>(foreachBody->getStmts()->at(1));
  }
};

// ===========================================================================
// package uvm / classes
// ===========================================================================

TEST_F(ForLoopBindTest, PackageUvmExists) { EXPECT_NE(getUvm(), nullptr); }

TEST_F(ForLoopBindTest, AllFiveClassesExist) {
  EXPECT_NE(getClass("uvm_vreg_cb_iter"), nullptr);
  EXPECT_NE(getClass("uvm_vreg_field_cb_iter"), nullptr);
  EXPECT_NE(getClass("uvm_vreg_field_cbs"), nullptr);
  EXPECT_NE(getClass("uvm_vreg_field"), nullptr);
  EXPECT_NE(getUvmVreg(), nullptr);
}

TEST_F(ForLoopBindTest, UvmVregHasFieldsQueueMember) {
  const hldb::ClassDefn *const cls = getUvmVreg();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("fields", cls->getVariables()), nullptr)
      << "'local uvm_vreg_field fields[$];' not found";
}

// ===========================================================================
// Sec 8.24: out-of-block "task uvm_vreg::write();" binds to the extern
// prototype and is reachable from uvm_vreg's own scope
// ===========================================================================

TEST_F(ForLoopBindTest, WriteTaskExistsInUvmVregScope) {
  const hldb::Task *const write = getWrite();
  ASSERT_NE(write, nullptr) << "Sec 8.24: out-of-block 'task uvm_vreg::write()' must bind to the matching "
                                "'extern task write();' prototype and live in uvm_vreg's scope";
  EXPECT_EQ(write->getName(), std::string_view{"write"});
}

TEST_F(ForLoopBindTest, WriteBodyStartsWithCbsVariableDecl) {
  const hldb::Begin *const body = getWriteBody();
  ASSERT_NE(body, nullptr) << "write()'s body should be a Begin (explicit begin-end in source)";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_GE(body->getStmts()->size(), 2u);
  const hldb::Variable *const cbs = any_cast<hldb::Variable>(body->getStmts()->at(0));
  ASSERT_NE(cbs, nullptr) << "'uvm_vreg_cb_iter cbs = new(this);' should be a Variable declaration statement";
  EXPECT_EQ(cbs->getName(), std::string_view{"cbs"});
}

// ===========================================================================
// Sec 12.7.3: foreach (fields[i])
// ===========================================================================

TEST_F(ForLoopBindTest, ForeachFieldsExists) { EXPECT_NE(getForeachFields(), nullptr); }

TEST_F(ForLoopBindTest, ForeachArrayNameIsFields) {
  const hldb::ForeachStmt *const fe = getForeachFields();
  ASSERT_NE(fe, nullptr);
  ASSERT_NE(fe->getVariable(), nullptr) << "12.7.3: array_name must be present";
  EXPECT_EQ(fe->getVariable()->getName(), std::string_view{"fields"});
}

TEST_F(ForLoopBindTest, ForeachHasOneIteratorLoopVarI) {
  const hldb::ForeachStmt *const fe = getForeachFields();
  ASSERT_NE(fe, nullptr);
  ASSERT_NE(fe->getLoopVars(), nullptr);
  ASSERT_EQ(fe->getLoopVars()->size(), 1u);
  const hldb::Variable *const i = any_cast<hldb::Variable>(fe->getLoopVars()->at(0));
  ASSERT_NE(i, nullptr);
  EXPECT_EQ(i->getName(), std::string_view{"i"});
  EXPECT_TRUE(i->getIsIterator());
}

TEST_F(ForLoopBindTest, ForeachBodyStartsWithShadowingCbsVariableDecl) {
  const hldb::Begin *const body = getForeachBody();
  ASSERT_NE(body, nullptr) << "foreach body should be a Begin (explicit begin-end in source)";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_GE(body->getStmts()->size(), 2u);
  const hldb::Variable *const cbs = any_cast<hldb::Variable>(body->getStmts()->at(0));
  ASSERT_NE(cbs, nullptr) << "'uvm_vreg_field_cb_iter cbs = new(fields[i]);' should be a Variable declaration "
                              "statement, scoped to the foreach body, shadowing the outer 'cbs'";
  EXPECT_EQ(cbs->getName(), std::string_view{"cbs"});
}

// ===========================================================================
// Sec 12.7.1: for (uvm_vreg_field_cbs cb = cbs; cb != null; cb = cbs)
// ===========================================================================

TEST_F(ForLoopBindTest, ForCbExists) { EXPECT_NE(getForCb(), nullptr); }

TEST_F(ForLoopBindTest, ForCbDeclaresLocalVariableCb) {
  const hldb::ForStmt *const fs = getForCb();
  ASSERT_NE(fs, nullptr);
  ASSERT_NE(fs->getVariables(), nullptr) << "12.7.1: 'uvm_vreg_field_cbs cb = cbs' is a for_variable_declaration, "
                                             "local to the ForStmt's own scope";
  EXPECT_NE(hldb::findByName<hldb::Variable>("cb", fs->getVariables()), nullptr);
}

TEST_F(ForLoopBindTest, ForCbConditionIsNotEqualOperation) {
  const hldb::ForStmt *const fs = getForCb();
  ASSERT_NE(fs, nullptr);
  const hldb::Operation *const cond = fs->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr) << "'cb != null' should be an Operation";
  EXPECT_EQ(cond->getOpType(), vpiNeqOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view{"cb"});
}

TEST_F(ForLoopBindTest, ForCbIncIsAssignCbEqualsCbs) {
  const hldb::ForStmt *const fs = getForCb();
  ASSERT_NE(fs, nullptr);
  ASSERT_NE(fs->getForIncStmts(), nullptr);
  ASSERT_EQ(fs->getForIncStmts()->size(), 1u);
  const hldb::Assignment *const inc = any_cast<hldb::Assignment>(fs->getForIncStmts()->at(0));
  ASSERT_NE(inc, nullptr) << "'cb = cbs' should be an Assignment";
  const hldb::RefObj *const lhs = inc->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view{"cb"});
  const hldb::RefObj *const rhs = inc->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), std::string_view{"cbs"});
}

// ---------------------------------------------------------------------------
// Innermost body: "cb.fname = 1;"
// ---------------------------------------------------------------------------

TEST_F(ForLoopBindTest, ForCbBodyIsCbFnameEqualsOne) {
  const hldb::ForStmt *const fs = getForCb();
  ASSERT_NE(fs, nullptr);
  const hldb::Begin *const body = fs->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "for-loop body should be a Begin (explicit begin-end in source)";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 1u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(body->getStmts()->at(0));
  ASSERT_NE(assign, nullptr) << "'cb.fname = 1;' should be an Assignment";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view{"cb.fname"});
  ASSERT_NE(assign->getRhs(), nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "'= 1' RHS should be a Constant";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
