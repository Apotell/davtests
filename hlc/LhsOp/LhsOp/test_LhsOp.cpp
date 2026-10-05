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

// Tests for dut.sv (tags: LhsOp)
//   module top(output logic a, output logic b);
//     typedef enum logic {c = 0, d = 1} cstate_e;
//      typedef struct packed {
//         cstate_e [1:0] class_esc_state;
//      } hw2reg_wrap_t;
//      hw2reg_wrap_t hw2reg_wrap = { c, d };
//      assign { a,
//               b } = hw2reg_wrap.class_esc_state;
//   endmodule
//
// What is checked (IEEE 1800-2023):
//   - 'output logic a/b' are ANSI output ports with a data type and no net
//     type, hence variables (23.2.2.3, 6.8)
//   - 'cstate_e' is a typedef of an enum (6.19) with base type logic and two
//     named constants c = 0, d = 1
//   - 'hw2reg_wrap_t' is a typedef of a packed struct (7.2.1) with a single
//     member 'class_esc_state' of packed array type cstate_e [1:0] (7.4.1)
//   - 'hw2reg_wrap' is a variable of that typedef, initialized (6.8) with
//     the concatenation {c, d} (11.4.12) whose operands refer to the enum
//     constants
//   - the continuous assignment (10.3.2) has a concatenation as its LHS
//     (11.4.12: "A concatenation can also be used as the target of an
//     assignment"), operands referring to variables a and b in order, and
//     the struct member path hw2reg_wrap.class_esc_state as its RHS (7.2)
//
// What is NOT checked and why:
//   - the object a struct-member reference binds to (TypespecMember vs. a
//     member object): not prescribed by the standard for non-elaborated
//     designs.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/enum.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class LhsOpTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "LhsOp.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Typedef *findTypedef(std::string_view name) {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getTypedefs() == nullptr) return nullptr;
    for (const hldb::Typedef *const td : *top->getTypedefs()) {
      if (td->getName() == name) return td;
    }
    return nullptr;
  }

  static const hldb::Enum *getEnum() {
    const hldb::Typedef *const td = findTypedef("cstate_e");
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    const hldb::EnumTypespec *const et = td->getAlias()->getActual<hldb::EnumTypespec>();
    return (et == nullptr) ? nullptr : et->getEnum();
  }

  static const hldb::EnumConst *findEnumConst(std::string_view name) {
    const hldb::Enum *const e = getEnum();
    if (e == nullptr || e->getEnumConsts() == nullptr) return nullptr;
    for (const hldb::EnumConst *const ec : *e->getEnumConsts()) {
      if (ec->getName() == name) return ec;
    }
    return nullptr;
  }

  static const hldb::Variable *findVar(std::string_view name) {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getVariables() == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>(name, top->getVariables());
  }
};

// ---------------------------------------------------------------------------
// Ports -- 23.2.2.3, 6.8
// ---------------------------------------------------------------------------

TEST_F(LhsOpTest, OutputLogicPortsAreVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  ASSERT_NE(top->getPorts(), nullptr);
  ASSERT_EQ(top->getPorts()->size(), 2u);
  const char *const names[] = {"a", "b"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::Port *const p = top->getPorts()->at(i);
    EXPECT_EQ(p->getName(), names[i]);
    EXPECT_EQ(p->getDirection(), vpiOutput);
    const hldb::RefObj *const low = p->getLowConn<hldb::RefObj>();
    ASSERT_NE(low, nullptr);
    ASSERT_NE(low->getActual(), nullptr);
    EXPECT_EQ(low->getActual()->getAnyType(), hldb::AnyType::Variable)
        << "23.2.2.3: 'output logic " << names[i] << "' (data type, no net type) is a variable";
  }
}

// ---------------------------------------------------------------------------
// typedef enum logic {c = 0, d = 1} cstate_e -- 6.19
// ---------------------------------------------------------------------------

TEST_F(LhsOpTest, CstateEIsEnumWithLogicBase) {
  const hldb::Enum *const e = getEnum();
  ASSERT_NE(e, nullptr) << "typedef 'cstate_e' must alias an enum";
  ASSERT_NE(e->getBaseTypespec(), nullptr);
  ASSERT_NE(e->getBaseTypespec()->getActual(), nullptr);
  EXPECT_EQ(e->getBaseTypespec()->getActual()->getAnyType(), hldb::AnyType::LogicTypespec)
      << "6.19: explicit base type 'logic'";
  ASSERT_NE(e->getEnumConsts(), nullptr);
  EXPECT_EQ(e->getEnumConsts()->size(), 2u);
}

TEST_F(LhsOpTest, EnumConstantValues) {
  const hldb::EnumConst *const c = findEnumConst("c");
  const hldb::EnumConst *const d = findEnumConst("d");
  ASSERT_NE(c, nullptr);
  ASSERT_NE(d, nullptr);
  const hldb::Constant *const cv = c->getValue<hldb::Constant>();
  const hldb::Constant *const dv = d->getValue<hldb::Constant>();
  ASSERT_NE(cv, nullptr);
  ASSERT_NE(dv, nullptr);
  EXPECT_EQ(cv->getValue(), "0");
  EXPECT_EQ(dv->getValue(), "1");
}

// ---------------------------------------------------------------------------
// typedef struct packed { cstate_e [1:0] class_esc_state; } -- 7.2.1, 7.4.1
// ---------------------------------------------------------------------------

TEST_F(LhsOpTest, Hw2regWrapTIsPackedStructWithPackedEnumArray) {
  const hldb::Typedef *const td = findTypedef("hw2reg_wrap_t");
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr);
  const hldb::Struct *const s = st->getStruct();
  ASSERT_NE(s, nullptr);
  EXPECT_TRUE(s->getPacked());
  ASSERT_NE(s->getMembers(), nullptr);
  ASSERT_EQ(s->getMembers()->size(), 1u);
  const hldb::TypespecMember *const m = s->getMembers()->at(0);
  EXPECT_EQ(m->getName(), "class_esc_state");
  ASSERT_NE(m->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const at = m->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr) << "'cstate_e [1:0]' is a packed array of cstate_e";
  EXPECT_TRUE(at->getPacked());
  ASSERT_NE(at->getRange(), nullptr);
  const hldb::Constant *const left = at->getRange()->getLeftExpr<hldb::Constant>();
  const hldb::Constant *const right = at->getRange()->getRightExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(left->getValue(), "1");
  EXPECT_EQ(right->getValue(), "0");
  ASSERT_NE(at->getElemTypespec(), nullptr);
  const hldb::TypedefTypespec *const elem = at->getElemTypespec()->getActual<hldb::TypedefTypespec>();
  ASSERT_NE(elem, nullptr);
  EXPECT_EQ(elem->getName(), "cstate_e");
}

// ---------------------------------------------------------------------------
// hw2reg_wrap_t hw2reg_wrap = { c, d }; -- 6.8, 11.4.12
// ---------------------------------------------------------------------------

TEST_F(LhsOpTest, Hw2regWrapIsVariableInitializedWithConcat) {
  const hldb::Variable *const v = findVar("hw2reg_wrap");
  ASSERT_NE(v, nullptr) << "6.8: 'hw2reg_wrap_t hw2reg_wrap' declares a variable";
  ASSERT_NE(v->getTypespec(), nullptr);
  const hldb::TypedefTypespec *const tt = v->getTypespec()->getActual<hldb::TypedefTypespec>();
  ASSERT_NE(tt, nullptr);
  EXPECT_EQ(tt->getName(), "hw2reg_wrap_t");

  const hldb::Operation *const init = v->getValue<hldb::Operation>();
  ASSERT_NE(init, nullptr) << "initializer '{ c, d }' must be an Operation";
  EXPECT_EQ(init->getOpType(), vpiConcatOp);
  ASSERT_NE(init->getOperands(), nullptr);
  ASSERT_EQ(init->getOperands()->size(), 2u);
  const hldb::RefObj *const c = any_cast<hldb::RefObj>(init->getOperands()->at(0));
  const hldb::RefObj *const d = any_cast<hldb::RefObj>(init->getOperands()->at(1));
  ASSERT_NE(c, nullptr);
  ASSERT_NE(d, nullptr);
  EXPECT_EQ(c->getName(), "c");
  EXPECT_EQ(d->getName(), "d");
  EXPECT_EQ(c->getActual(), findEnumConst("c"));
  EXPECT_EQ(d->getActual(), findEnumConst("d"));
}

// ---------------------------------------------------------------------------
// assign {a, b} = hw2reg_wrap.class_esc_state; -- 10.3.2, 11.4.12, 7.2
// ---------------------------------------------------------------------------

TEST_F(LhsOpTest, ContAssignLhsIsConcatOfAB) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = top->getContAssigns()->at(0);
  const hldb::Operation *const lhs = ca->getLhs<hldb::Operation>();
  ASSERT_NE(lhs, nullptr) << "11.4.12: the LHS '{a, b}' is a concatenation";
  EXPECT_EQ(lhs->getOpType(), vpiConcatOp);
  ASSERT_NE(lhs->getOperands(), nullptr);
  ASSERT_EQ(lhs->getOperands()->size(), 2u);
  const hldb::RefObj *const a = any_cast<hldb::RefObj>(lhs->getOperands()->at(0));
  const hldb::RefObj *const b = any_cast<hldb::RefObj>(lhs->getOperands()->at(1));
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(a->getName(), "a");
  EXPECT_EQ(b->getName(), "b");
  EXPECT_EQ(a->getActual(), findVar("a"));
  EXPECT_EQ(b->getActual(), findVar("b"));
}

TEST_F(LhsOpTest, ContAssignRhsIsStructMemberPath) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 1u);
  const hldb::RefObj *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getPathElems(), nullptr);
  ASSERT_EQ(rhs->getPathElems()->size(), 2u) << "hw2reg_wrap . class_esc_state";
  const hldb::RefObj *const root = any_cast<hldb::RefObj>(rhs->getPathElems()->at(0));
  const hldb::RefObj *const leaf = any_cast<hldb::RefObj>(rhs->getPathElems()->at(1));
  ASSERT_NE(root, nullptr);
  ASSERT_NE(leaf, nullptr);
  EXPECT_EQ(root->getName(), "hw2reg_wrap");
  EXPECT_EQ(root->getActual(), findVar("hw2reg_wrap"));
  EXPECT_EQ(leaf->getName(), "class_esc_state");
  EXPECT_NE(leaf->getActual(), nullptr) << "member must resolve";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
