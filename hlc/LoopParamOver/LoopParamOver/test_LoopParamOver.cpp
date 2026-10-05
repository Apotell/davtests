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

// Tests for tests/LoopParamOver/dut.sv (tags: LoopParamOver)
//    1 module Foo ();
//    2  parameter P1 = 10;
//    3  parameter P2 = P1 + P2;
//    5 endmodule
//    8 module top();
//    9   parameter P3 = P1;
//   10   parameter P2 = P3;
//   11   parameter P1 = P2;
//   12 Foo #(.P1(P2)) sub();
//   13 endmodule
//
// This file is deliberately illegal: Foo.P2 depends on itself, and in top
// P3 -> P1 -> P2 -> P3 form a dependency cycle. The by-name override of
// Foo.P1 with top.P2 does not break Foo.P2's self-dependency.
//
// What is checked (IEEE 1800-2023):
//   - 6.20.4: neither module has a parameter_port_list, so every body
//     "parameter" is nonlocal.
//   - shape: Foo.P2 = Operation(vpiAddOp, RefObj "P1", RefObj "P2");
//     top's P3 = "P1", P2 = "P3", P1 = "P2".
//   - 23.3.1: top is the only top-level module (Foo is instantiated).
//   - 23.10.2.2 parameter value assignment by name: sub overrides Foo.P1;
//     the override LHS binds to Foo.P1 and the RHS expression "P2" is
//     evaluated in the instantiating scope, so it binds to top.P2.
//   - 6.5 "Data shall be declared before they are used" with 6.20
//     "Constants are named data objects": "P1" on line 9 precedes top.P1's
//     declaration on line 11 -- COMP_FAILED_TO_BIND for "P1" at line 9.
//     Backward references (line 10 "P3", line 11 "P2", line 3 "P1") are
//     legal and must bind to the parameter of the same module.
//   - 6.20 / 23.10.4 (elaboration "computes parameter values"): a parameter
//     whose value depends on itself has no value. Both cycles must be
//     diagnosed with ELAB_EXPRESSION_LOOP, located at the parameter
//     assignments that close them (Foo line 3; top line 9).
//
// What is NOT checked and why:
//   - the binding target of "P1" on line 9: per 6.5 the forward reference
//     is illegal, so there is no standard-correct target to assert.
//   - which binding the self-reference "P2" on line 3 should get: the
//     reference sits inside P2's own declaration; the defect is the
//     circular value dependency, checked via ELAB_EXPRESSION_LOOP.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class LoopParamOverTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "LoopParamOver.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }
  static const hldb::Parameter *getParam(std::string_view module, std::string_view name) {
    const hldb::Module *const m = getModule(module);
    if (m == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, m->getParameters());
  }
  static const hldb::ParamAssign *getParamAssign(std::string_view module, std::string_view name) {
    const hldb::Module *const m = getModule(module);
    if (m == nullptr) return nullptr;
    return hldb::findByName<hldb::ParamAssign>(name, m->getParamAssigns());
  }
};

TEST_F(LoopParamOverTest, ModulesExist) {
  EXPECT_NE(getModule("Foo"), nullptr);
  EXPECT_NE(getModule("top"), nullptr);
}

TEST_F(LoopParamOverTest, AllParametersAreNonlocal) {
  for (std::string_view name : {"P1", "P2"}) {
    const hldb::Parameter *const p = getParam("Foo", name);
    ASSERT_NE(p, nullptr) << "Foo." << name;
    EXPECT_FALSE(p->getLocalParam()) << "Foo." << name;
  }
  for (std::string_view name : {"P1", "P2", "P3"}) {
    const hldb::Parameter *const p = getParam("top", name);
    ASSERT_NE(p, nullptr) << "top." << name;
    EXPECT_FALSE(p->getLocalParam()) << "top." << name;
  }
}

// 23.3.1
TEST_F(LoopParamOverTest, OnlyTopIsTopLevelModule) {
  const hldb::Module *const top = getModule("top");
  const hldb::Module *const foo = getModule("Foo");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(foo, nullptr);
  EXPECT_TRUE(top->getTopModule());
  EXPECT_FALSE(foo->getTopModule()) << "23.3.1: Foo appears in an instantiation statement";
}

TEST_F(LoopParamOverTest, FooP1DefaultIs10) {
  const hldb::ParamAssign *const pa = getParamAssign("Foo", "P1");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDecompile(), "10");
}

TEST_F(LoopParamOverTest, FooP2IsSelfReferentialAddition) {
  const hldb::ParamAssign *const pa = getParamAssign("Foo", "P2");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const op = pa->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiAddOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  const hldb::RefObj *const a = any_cast<hldb::RefObj>(op->getOperands()->at(0));
  const hldb::RefObj *const b = any_cast<hldb::RefObj>(op->getOperands()->at(1));
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(a->getName(), "P1");
  EXPECT_EQ(b->getName(), "P2");
  // 6.5: P1 (line 2) is declared before line 3 and must bind to Foo.P1.
  ASSERT_NE(a->getActual(), nullptr);
  EXPECT_EQ(a->getActual(), getParam("Foo", "P1"));
}

// top: P3 = P1; P2 = P3; P1 = P2;
TEST_F(LoopParamOverTest, TopParamAssignShapes) {
  struct Expected {
    std::string_view m_lhs;
    std::string_view m_rhs;
    uint32_t m_line;
  };
  const Expected expected[] = {{"P3", "P1", 9}, {"P2", "P3", 10}, {"P1", "P2", 11}};
  for (const Expected &e : expected) {
    const hldb::ParamAssign *const pa = getParamAssign("top", e.m_lhs);
    ASSERT_NE(pa, nullptr) << e.m_lhs;
    const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr) << e.m_lhs;
    EXPECT_EQ(lhs->getActual(), getParam("top", e.m_lhs)) << e.m_lhs;
    const hldb::RefObj *const rhs = pa->getRhs<hldb::RefObj>();
    ASSERT_NE(rhs, nullptr) << e.m_lhs;
    EXPECT_EQ(rhs->getName(), e.m_rhs) << e.m_lhs;
    EXPECT_EQ(rhs->getStartLine(), e.m_line) << e.m_lhs;
  }
}

// 6.5: backward references in top are legal and bind.
TEST_F(LoopParamOverTest, TopBackwardReferencesBind) {
  const hldb::ParamAssign *const p2 = getParamAssign("top", "P2");
  ASSERT_NE(p2, nullptr);
  const hldb::RefObj *const r2 = p2->getRhs<hldb::RefObj>();
  ASSERT_NE(r2, nullptr);
  ASSERT_NE(r2->getActual(), nullptr);
  EXPECT_EQ(r2->getActual(), getParam("top", "P3"));

  const hldb::ParamAssign *const p1 = getParamAssign("top", "P1");
  ASSERT_NE(p1, nullptr);
  const hldb::RefObj *const r1 = p1->getRhs<hldb::RefObj>();
  ASSERT_NE(r1, nullptr);
  ASSERT_NE(r1->getActual(), nullptr);
  EXPECT_EQ(r1->getActual(), getParam("top", "P2"));
}

// 23.10.2.2: Foo #(.P1(P2)) sub();
TEST_F(LoopParamOverTest, SubOverridesFooP1WithTopP2) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  const hldb::RefInstance *const sub = hldb::findByName<hldb::RefInstance>("sub", top->getRefInstances());
  ASSERT_NE(sub, nullptr);
  ASSERT_NE(sub->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = sub->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getModule(), getModule("Foo"));
  ASSERT_NE(mt->getParamAssigns(), nullptr);
  ASSERT_EQ(mt->getParamAssigns()->size(), 1u);
  const hldb::ParamAssign *const pa = mt->getParamAssigns()->at(0);
  ASSERT_NE(pa, nullptr);
  EXPECT_TRUE(pa->getConnByName());
  EXPECT_TRUE(pa->getOverridden());
  const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "P1");
  EXPECT_EQ(lhs->getActual(), getParam("Foo", "P1"));
  const hldb::RefObj *const rhs = pa->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "P2");
  EXPECT_EQ(rhs->getActual(), getParam("top", "P2")) << "override value is evaluated in the instantiating scope";
}

// 6.5 + 6.20: "P1" on line 9 is used before top.P1 is declared on line 11.
TEST_F(LoopParamOverTest, ForwardReferenceToP1IsDiagnosed) {
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "P1", 9), nullptr)
      << "6.5: data shall be declared before they are used; top.P1 is declared on line 11";
}

// 6.20 / 23.10.4: Foo.P2 = P1 + P2 has no computable value.
TEST_F(LoopParamOverTest, FooSelfDependencyIsDiagnosed) {
  EXPECT_NE(findError(ErrorDefinition::ELAB_EXPRESSION_LOOP, 3), nullptr)
      << "Foo.P2 depends on itself; overriding Foo.P1 does not remove the loop";
}

// 6.20 / 23.10.4: top.P3 -> P1 -> P2 -> P3 has no computable value.
TEST_F(LoopParamOverTest, TopCircularDependencyIsDiagnosed) {
  EXPECT_NE(findError(ErrorDefinition::ELAB_EXPRESSION_LOOP, 9), nullptr)
      << "top.P3 = P1, P2 = P3, P1 = P2 form a dependency loop";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
