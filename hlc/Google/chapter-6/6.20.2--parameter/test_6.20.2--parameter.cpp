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

// Spec-based validation of IEEE 1800-2017 §6.20.2 module parameter.
// SV: tests/Google/chapter-6/6.20.2--parameter.sv
//
//   module top();
//       parameter p = 123;
//   endmodule
//
// ── §6.20.2 constructs under test ────────────────────────────────────────────
//
// A parameter declared without an explicit type and initialised with an
// unsized decimal integer literal (IEEE 1800-2017 §6.20.2, §5.7.1):
//   • Has an inferred LogicTypespec (Surelog default for untyped parameters).
//   • Its default value "123" is an unsigned integer constant
//     (vpiConstType = vpiUIntConst = 9) with size 64.
//   • The elaborator creates exactly one ParamAssign binding the parameter
//     name (LHS RefObj → actual Parameter) to its value (RHS Constant).
//
// ── UHDM tree (from log) ──────────────────────────────────────────────────
//
//   Design name:unnamed
//   └── vpiAllModules (1 item)
//       └── Module name:work@top
//           ├── vpiParameter (1 item)
//           │   └── Parameter name:p
//           │       └── vpiTypespec  RefTypespec → actual: LogicTypespec
//           └── vpiParamAssign (1 item)
//               └── ParamAssign
//                   ├── vpiLhs  RefObj name:p → actual: Parameter name:p
//                   └── vpiRhs  Constant
//                       ├── vpiTypespec  RefTypespec → actual: IntTypespec
//                       ├── vpiConstType: unsigned int (9)
//                       ├── vpiSize: 64
//                       └── vpiDecompile: "123"
//
// ── VPI constants ─────────────────────────────────────────────────────────
//   vpiUIntConst = 9

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/constant.h>
#include <uhdm/design.h>
#include <uhdm/logic_typespec.h>
#include <uhdm/module.h>
#include <uhdm/param_assign.h>
#include <uhdm/parameter.h>
#include <uhdm/ref_obj.h>
#include <uhdm/ref_typespec.h>

#include <string>

namespace SURELOG {

class ParameterTest : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.20.2--parameter.hlc"});

    ASSERT_NE(m_session,  nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design,   nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const uhdm::Module *getTop(const uhdm::Design *d) {
  return uhdm::findByName<uhdm::Module>("work@top", d->getAllModules());
}

static const uhdm::Parameter *getParam(const uhdm::Design *d) {
  const uhdm::Module *m = getTop(d);
  if (!m || !m->getParameters()) return nullptr;
  return uhdm::findByName<uhdm::Parameter>("p", m->getParameters());
}

static const uhdm::ParamAssign *getParamAssign(const uhdm::Design *d) {
  const uhdm::Module *m = getTop(d);
  if (!m || !m->getParamAssigns() || m->getParamAssigns()->empty())
    return nullptr;
  return (*m->getParamAssigns())[0];
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(ParameterTest, ModuleExists) {
  EXPECT_NE(getTop(m_design), nullptr);
}

// ===========================================================================
// Parameter collection
// ===========================================================================

TEST_F(ParameterTest, Parameter_Collection_HasOneEntry) {
  const uhdm::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParameters(), nullptr);
  EXPECT_EQ(m->getParameters()->size(), 1u);
}

TEST_F(ParameterTest, Parameter_p_Exists) {
  EXPECT_NE(getParam(m_design), nullptr);
}

// IEEE 1800-2017 §6.20.2: a parameter without an explicit type is inferred
// as logic; Surelog represents this as LogicTypespec via RefTypespec.
TEST_F(ParameterTest, Parameter_p_HasLogicTypespec) {
  const uhdm::Parameter *p = getParam(m_design);
  ASSERT_NE(p, nullptr);
  const uhdm::RefTypespec *rt = p->getTypespec();
  ASSERT_NE(rt, nullptr) << "Parameter 'p' must have a RefTypespec";
  EXPECT_NE(rt->getActual<uhdm::LogicTypespec>(), nullptr)
      << "RefTypespec must resolve to LogicTypespec for an untyped parameter";
}

// ===========================================================================
// ParamAssign collection
// ===========================================================================

TEST_F(ParameterTest, ParamAssign_Collection_HasOneEntry) {
  const uhdm::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParamAssigns(), nullptr);
  EXPECT_EQ(m->getParamAssigns()->size(), 1u);
}

// ===========================================================================
// ParamAssign LHS
// ===========================================================================

TEST_F(ParameterTest, ParamAssign_Lhs_IsRefObj) {
  const uhdm::ParamAssign *pa = getParamAssign(m_design);
  ASSERT_NE(pa, nullptr);
  EXPECT_NE(pa->getLhs<uhdm::RefObj>(), nullptr)
      << "ParamAssign LHS must be a RefObj";
}

TEST_F(ParameterTest, ParamAssign_Lhs_NameIsP) {
  const uhdm::ParamAssign *pa = getParamAssign(m_design);
  ASSERT_NE(pa, nullptr);
  const uhdm::RefObj *lhs = pa->getLhs<uhdm::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "p");
}

// The LHS RefObj's vpiActual must resolve to the Parameter declaration.
TEST_F(ParameterTest, ParamAssign_Lhs_ActualIsParameter) {
  const uhdm::ParamAssign *pa = getParamAssign(m_design);
  ASSERT_NE(pa, nullptr);
  const uhdm::RefObj *lhs = pa->getLhs<uhdm::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_NE(lhs->getActual<uhdm::Parameter>(), nullptr)
      << "LHS RefObj must resolve to a Parameter";
}

// ===========================================================================
// ParamAssign RHS
// ===========================================================================

TEST_F(ParameterTest, ParamAssign_Rhs_IsConstant) {
  const uhdm::ParamAssign *pa = getParamAssign(m_design);
  ASSERT_NE(pa, nullptr);
  EXPECT_NE(pa->getRhs<uhdm::Constant>(), nullptr)
      << "ParamAssign RHS must be a Constant";
}

// IEEE 1800-2017 §5.7.1: unsized decimal integer literals without a sign
// qualifier are unsigned; Surelog encodes this as vpiUIntConst (9).
TEST_F(ParameterTest, ParamAssign_Rhs_ConstType_IsUnsignedInt) {
  const uhdm::ParamAssign *pa = getParamAssign(m_design);
  ASSERT_NE(pa, nullptr);
  const uhdm::Constant *rhs = pa->getRhs<uhdm::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getConstType(), vpiUIntConst)
      << "Constant for '123' must have vpiConstType == vpiUIntConst (9)";
}

// The value string must reproduce the exact source literal.
TEST_F(ParameterTest, ParamAssign_Rhs_ValueIs_123) {
  const uhdm::ParamAssign *pa = getParamAssign(m_design);
  ASSERT_NE(pa, nullptr);
  const uhdm::Constant *rhs = pa->getRhs<uhdm::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(std::string(rhs->getValue()), "123");
}

// IEEE 1800-2017 §6.20.2: an untyped parameter takes its size from the
// constant expression. An unsized decimal literal uses the host integer
// width; Surelog represents this as 64 bits.
TEST_F(ParameterTest, ParamAssign_Rhs_SizeIs64) {
  const uhdm::ParamAssign *pa = getParamAssign(m_design);
  ASSERT_NE(pa, nullptr);
  const uhdm::Constant *rhs = pa->getRhs<uhdm::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getSize(), 64)
      << "unsized decimal literal in untyped parameter must have host-int size (64)";
}

}  // namespace SURELOG
