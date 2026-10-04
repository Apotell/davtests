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

// Tests for dut.sv (tags: ForeachClass)
//   package uvm;
//     class uvm_reg_map;
//       local logic m_mems_by_offset[10];
//     endclass
//     function f1();
//       uvm_reg_map top_map;
//       foreach (top_map.m_mems_by_offset[range]) begin
//         if (addrs[i] >= range.min && addrs[i] <= range.max) begin
//         end
//       end
//     endfunction
//   endpackage
//
// This exercises a "foreach" loop (IEEE 1800-2023 Sec 12.7.3) whose array
// expression is a class-member array reached through a class-typed handle
// ("top_map.m_mems_by_offset"), i.e. the array being iterated is a member
// of a class, not a plain local array.
//
// "addrs" and "i" inside the loop body are never declared anywhere in this
// file (deliberately, like other malformed-fragment tests in this suite):
// per 6.3/23.6 both must fail to bind. Likewise "range" is the implicitly
// declared foreach iterator (typed from the array's index range, i.e. an
// int-like iterator -- see 12.7.3), not a handle with "min"/"max" members,
// so "range.min" and "range.max" cannot resolve to a method call and are
// expected to report a null actual for the unresolved "min"/"max" path
// elements. None of this affects the foreach construct's own shape, which
// is what this file actually tests.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/class_defn.h>
#include <hldb/foreach_stmt.h>
#include <hldb/function.h>
#include <hldb/package.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ForeachClassTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ForeachClass.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getUvm() {
    return hldb::findByName<hldb::Package>("uvm", m_design->getAllPackages());
  }

  static const hldb::ClassDefn *getUvmRegMap() {
    const hldb::Package *const uvm = getUvm();
    if (uvm == nullptr || uvm->getClassDefns() == nullptr) return nullptr;
    return hldb::findByName<hldb::ClassDefn>("uvm_reg_map", uvm->getClassDefns());
  }

  static const hldb::Function *getF1() {
    const hldb::Package *const uvm = getUvm();
    if (uvm == nullptr || uvm->getTaskFuncs() == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("f1", uvm->getTaskFuncs());
  }

  static const hldb::ForeachStmt *getForeach() {
    const hldb::Function *const f1 = getF1();
    if (f1 == nullptr) return nullptr;
    const hldb::Begin *const body = f1->getStmt<hldb::Begin>();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->size() < 2) return nullptr;
    return any_cast<hldb::ForeachStmt>(body->getStmts()->at(1));
  }
};

// ===========================================================================
// package uvm / class uvm_reg_map / function f1
// ===========================================================================

TEST_F(ForeachClassTest, PackageUvmExists) { EXPECT_NE(getUvm(), nullptr); }

TEST_F(ForeachClassTest, ClassUvmRegMapHasMemberArray) {
  const hldb::ClassDefn *const cls = getUvmRegMap();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  const hldb::Variable *const member = hldb::findByName<hldb::Variable>("m_mems_by_offset", cls->getVariables());
  ASSERT_NE(member, nullptr) << "class 'uvm_reg_map' should declare member 'm_mems_by_offset'";
  EXPECT_EQ(member->getName(), std::string_view{"m_mems_by_offset"});
}

TEST_F(ForeachClassTest, FunctionF1Exists) {
  const hldb::Function *const f1 = getF1();
  ASSERT_NE(f1, nullptr);
  EXPECT_EQ(f1->getName(), std::string_view{"f1"});
}

// ===========================================================================
// f1's body is a Begin containing [0] the 'top_map' variable declaration,
// [1] the ForeachStmt (12.7.3)
// ===========================================================================

TEST_F(ForeachClassTest, F1BodyHasVariableDeclThenForeach) {
  const hldb::Function *const f1 = getF1();
  ASSERT_NE(f1, nullptr);
  const hldb::Begin *const body = f1->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "f1's body should be a Begin (local var decl + foreach)";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 2u);

  const hldb::Variable *const decl = any_cast<hldb::Variable>(body->getStmts()->at(0));
  ASSERT_NE(decl, nullptr) << "first statement should be the 'top_map' local variable declaration";
  EXPECT_EQ(decl->getName(), std::string_view{"top_map"});

  const hldb::ForeachStmt *const fe = getForeach();
  ASSERT_NE(fe, nullptr) << "second statement should be the ForeachStmt";
  EXPECT_EQ(fe->getAnyType(), hldb::AnyType::ForeachStmt);
}

// ===========================================================================
// 12.7.3: foreach array expression is 'top_map.m_mems_by_offset', a
// class-member array reached through the 'top_map' handle
// ===========================================================================

TEST_F(ForeachClassTest, ForeachArrayIsClassMemberPath) {
  const hldb::ForeachStmt *const fe = getForeach();
  ASSERT_NE(fe, nullptr);
  const hldb::RefObj *const arr = fe->getVariable();
  ASSERT_NE(arr, nullptr) << "12.7.3: foreach shall name the array being iterated";
  EXPECT_EQ(arr->getName(), std::string_view{"top_map.m_mems_by_offset"});
  ASSERT_NE(arr->getPathElems(), nullptr);
  ASSERT_EQ(arr->getPathElems()->size(), 2u);
  const hldb::RefObj *const handle = any_cast<hldb::RefObj>(arr->getPathElems()->at(0));
  ASSERT_NE(handle, nullptr);
  EXPECT_EQ(handle->getName(), std::string_view{"top_map"});
  const hldb::RefObj *const member = any_cast<hldb::RefObj>(arr->getPathElems()->at(1));
  ASSERT_NE(member, nullptr);
  EXPECT_EQ(member->getName(), std::string_view{"m_mems_by_offset"});
}

// ===========================================================================
// 12.7.3: exactly one loop variable ('range') is declared for the
// single-dimension array, implicitly typed and marked as an iterator
// ===========================================================================

TEST_F(ForeachClassTest, ForeachHasOneIteratorLoopVar) {
  const hldb::ForeachStmt *const fe = getForeach();
  ASSERT_NE(fe, nullptr);
  ASSERT_NE(fe->getLoopVars(), nullptr);
  ASSERT_EQ(fe->getLoopVars()->size(), 1u);
  const hldb::Variable *const range = any_cast<hldb::Variable>(fe->getLoopVars()->at(0));
  ASSERT_NE(range, nullptr);
  EXPECT_EQ(range->getName(), std::string_view{"range"});
  EXPECT_TRUE(range->getIsIterator()) << "12.7.3: foreach loop variables are implicitly declared iterators";
}

// ===========================================================================
// foreach body exists (an if-statement wrapped in Begin, per the source)
// ===========================================================================

TEST_F(ForeachClassTest, ForeachBodyExists) {
  const hldb::ForeachStmt *const fe = getForeach();
  ASSERT_NE(fe, nullptr);
  const hldb::Begin *const body = fe->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "foreach body should be a Begin (explicit begin-end in source)";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 1u);
}

// ===========================================================================
// 6.3: 'addrs' and 'i' are never declared -- must fail to bind
// ===========================================================================

TEST_F(ForeachClassTest, UndeclaredIdentifiersFailToBind) {
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "addrs"), nullptr) << "'addrs' is never declared";
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "i"), nullptr) << "'i' is never declared";
}

// ===========================================================================
// 'range' is the implicit int-like foreach iterator, not a handle -- so
// 'range.min'/'range.max' cannot resolve as method calls
// ===========================================================================

TEST_F(ForeachClassTest, RangeMinMaxFailToResolve) {
  EXPECT_NE(findError(ErrorDefinition::LINT_NULL_ACTUAL, "min", 12, 29), nullptr)
      << "'range.min' should not resolve: 'range' is a foreach iterator, not a class handle";
  EXPECT_NE(findError(ErrorDefinition::LINT_NULL_ACTUAL, "max", 12, 54), nullptr)
      << "'range.max' should not resolve: 'range' is a foreach iterator, not a class handle";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
