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

// Tests for tests/LoopParam/dut.sv (tags: LoopParam)
//   3 module Foo ();
//   4  parameter P1 = P2;
//   5  parameter P2 = P1;
//   7 endmodule
//
// This file is deliberately illegal: the two parameters are defined in
// terms of each other, so neither has a value.
//
// What is checked (IEEE 1800-2023):
//   - 6.20.1/6.20.2: module Foo declares two value parameters P1 and P2;
//     6.20.4: Foo has no parameter_port_list, so both are nonlocal.
//   - the parameter assignments keep their source shape: P1 = RefObj "P2"
//     (line 4) and P2 = RefObj "P1" (line 5).
//   - 6.5 "Data shall be declared before they are used, apart from implicit
//     nets" together with 6.20 "Constants are named data objects": the
//     reference to P2 on line 4 precedes P2's declaration on line 5 and so
//     cannot bind -- COMP_FAILED_TO_BIND for "P2" at line 4.
//   - 6.20 / 23.10.4: parameters are elaboration-time constants whose values
//     are computed during elaboration ("computes parameter values"). P1 and
//     P2 depend on each other circularly, so no value can be computed and
//     the loop must be diagnosed (ELAB_EXPRESSION_LOOP; this is the only
//     expression loop in the file, so the type-only lookup is unambiguous).
//   - P1 on line 5 refers to an already declared parameter and must bind
//     (no COMP_FAILED_TO_BIND for "P1" at line 5).
//
// What is NOT checked and why:
//   - the RefObj::getActual() of "P2" on line 4: per 6.5 the reference is
//     illegal, so there is no standard-correct binding target to assert.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>

namespace hlc {

class LoopParamTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "LoopParam.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getFoo() { return hldb::findByName<hldb::Module>("Foo", m_design->getAllModules()); }
};

TEST_F(LoopParamTest, ModuleFooExists) { EXPECT_NE(getFoo(), nullptr); }

TEST_F(LoopParamTest, FooDeclaresTwoNonlocalParameters) {
  const hldb::Module *const foo = getFoo();
  ASSERT_NE(foo, nullptr);
  ASSERT_NE(foo->getParameters(), nullptr);
  EXPECT_EQ(foo->getParameters()->size(), 2u);
  for (std::string_view name : {"P1", "P2"}) {
    const hldb::Parameter *const p = hldb::findByName<hldb::Parameter>(name, foo->getParameters());
    ASSERT_NE(p, nullptr) << name;
    EXPECT_FALSE(p->getLocalParam()) << name << ": 6.20.4 no parameter_port_list -> nonlocal";
  }
}

TEST_F(LoopParamTest, P1IsAssignedReferenceToP2) {
  const hldb::Module *const foo = getFoo();
  ASSERT_NE(foo, nullptr);
  const hldb::ParamAssign *const pa = hldb::findByName<hldb::ParamAssign>("P1", foo->getParamAssigns());
  ASSERT_NE(pa, nullptr);
  const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getActual(), hldb::findByName<hldb::Parameter>("P1", foo->getParameters()));
  const hldb::RefObj *const rhs = pa->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "P2");
  EXPECT_EQ(rhs->getStartLine(), 4u);
}

TEST_F(LoopParamTest, P2IsAssignedReferenceToP1) {
  const hldb::Module *const foo = getFoo();
  ASSERT_NE(foo, nullptr);
  const hldb::ParamAssign *const pa = hldb::findByName<hldb::ParamAssign>("P2", foo->getParamAssigns());
  ASSERT_NE(pa, nullptr);
  const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getActual(), hldb::findByName<hldb::Parameter>("P2", foo->getParameters()));
  const hldb::RefObj *const rhs = pa->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "P1");
  EXPECT_EQ(rhs->getStartLine(), 5u);
}

// 6.5: backward reference to an already declared parameter is legal.
TEST_F(LoopParamTest, BackwardReferenceToP1Binds) {
  const hldb::Module *const foo = getFoo();
  ASSERT_NE(foo, nullptr);
  const hldb::ParamAssign *const pa = hldb::findByName<hldb::ParamAssign>("P2", foo->getParamAssigns());
  ASSERT_NE(pa, nullptr);
  const hldb::RefObj *const rhs = pa->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getActual(), nullptr);
  EXPECT_EQ(rhs->getActual(), hldb::findByName<hldb::Parameter>("P1", foo->getParameters()));
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "P1", 5), nullptr);
}

// 6.5 + 6.20: "P2" on line 4 is used before its declaration on line 5.
TEST_F(LoopParamTest, ForwardReferenceToP2IsDiagnosed) {
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "P2", 4), nullptr)
      << "6.5: data shall be declared before they are used; P2 is declared on line 5";
}

// 6.20 / 23.10.4: P1 <-> P2 have no computable value.
TEST_F(LoopParamTest, CircularParameterDependencyIsDiagnosed) {
  EXPECT_NE(findError(ErrorDefinition::ELAB_EXPRESSION_LOOP), nullptr)
      << "P1 = P2 and P2 = P1 form a dependency loop; neither parameter has a value";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
