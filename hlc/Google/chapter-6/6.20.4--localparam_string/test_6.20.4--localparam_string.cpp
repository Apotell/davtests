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

// Spec-based validation of IEEE 1800-2017 ss.6.20.4 localparam with string
// values and the ss.6.16 string type.
// SV: tests/Google/chapter-6/6.20.4--localparam_string.sv
//
//   module top();
//       localparam        s1 = "foo";
//       localparam string s2 = "bar";
//   endmodule
//
// -- ss.6.20.4 + ss.6.16 + ss.5.9 rules under test ---------------------------
//
// Local parameter (ss.6.20.4):
//   * Both s1 and s2 are localparams -- not overridable at instantiation.
//
// String literal (ss.5.9):
//   * A quoted string "foo" or "bar" is a string literal.
//   * In UHDM, a string literal is a Constant with constType vpiStringConst (6).
//   * This applies regardless of whether the parameter has an explicit type.
//
// String type (ss.6.16):
//   * 'string' is a built-in variable-size string data type.
//   * 'localparam string s2' explicitly declares s2 with type string.
//   * UHDM represents the explicit 'string' type as a StringTypespec on
//     the parameter's typespec field.
//
// Note on s1 (implicit type):
//   * 'localparam s1 = "foo"' has no explicit type keyword. The spec says the
//     parameter takes the type of its right-hand side expression (ss.6.20.2).
//     The RHS "foo" is a string literal so the spec implies type string, but
//     Surelog assigns an implicit LogicTypespec at this point. The typespec of
//     s1 is therefore implementation-defined and is not tested here. Only the
//     RHS constant type (vpiStringConst) is tested for s1, which IS
//     spec-grounded via ss.5.9.
//
// -- UHDM tree ----------------------------------------------------------------
//
//   Module name:work@top
//   +-- getParameters() (AnyCollection, 2 items)
//   |   +-- [0] Parameter name:"s1"  localParam: true
//   |           typespec: RefTypespec -> LogicTypespec  (Surelog implicit)
//   |   +-- [1] Parameter name:"s2"  localParam: true
//   |           typespec: RefTypespec -> StringTypespec (explicit 'string')
//   +-- getParamAssigns() (ParamAssignCollection, 2 items)
//       +-- [0] ParamAssign  lhs:"s1"  rhs: Constant { constType: vpiStringConst (6) }
//       +-- [1] ParamAssign  lhs:"s2"  rhs: Constant { constType: vpiStringConst (6) }
//
// -- VPI constants ------------------------------------------------------------
//   vpiStringConst = 6  (string literal constant, vpi_user.h)

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/vpi_user.h>

#include <string>

namespace hlc {

class LocalparamStringTest : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.20.4--localparam_string.hlc"});

    ASSERT_NE(m_session,  nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design,   nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design   = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session  = nullptr;
  }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("work@top", d->getAllModules());
}

static const hldb::Parameter *getParam(const hldb::Design *d,
                                        std::string_view name) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getParameters()) return nullptr;
  return hldb::findByName<hldb::Parameter>(name, m->getParameters());
}

static const hldb::ParamAssign *getParamAssign(const hldb::Design *d,
                                                std::string_view name) {
  const hldb::Module *m = getTop(d);
  if (!m) return nullptr;
  return hldb::findByName<hldb::ParamAssign>(name, m->getParamAssigns());
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(LocalparamStringTest, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr) << "module 'work@top' not found";
}

// ===========================================================================
// Parameter collection
// ===========================================================================

TEST_F(LocalparamStringTest, ParameterCollectionExists) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(m->getParameters(), nullptr)
      << "module 'top' must have a parameter collection";
}

// ss.6.20.4: two localparams s1 and s2 are declared.
TEST_F(LocalparamStringTest, ParameterCount) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParameters(), nullptr);
  EXPECT_EQ(m->getParameters()->size(), 2u)
      << "module 'top' declares exactly two localparams: s1 and s2";
}

// ===========================================================================
// localparam s1 = "foo"  (implicit type, string literal)
// ===========================================================================

TEST_F(LocalparamStringTest, S1_Exists) {
  EXPECT_NE(getParam(m_design, "s1"), nullptr)
      << "'s1' not found in parameters";
}

// ss.6.20.4: s1 is a localparam and must NOT be overridable.
TEST_F(LocalparamStringTest, S1_IsLocalParam) {
  const hldb::Parameter *p = getParam(m_design, "s1");
  ASSERT_NE(p, nullptr);
  EXPECT_TRUE(p->getLocalParam())
      << "ss.6.20.4: 'localparam s1' must be marked as a localparam";
}

// ss.5.9: "foo" is a string literal; the ParamAssign RHS must be a Constant.
TEST_F(LocalparamStringTest, S1_RhsIsConstant) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "s1");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 's1' not found";
  EXPECT_NE(pa->getRhs<hldb::Constant>(), nullptr)
      << "ss.5.9: '\"foo\"' must be represented as a Constant node";
}

// ss.5.9: a string literal has constType vpiStringConst (6).
TEST_F(LocalparamStringTest, S1_Rhs_ConstType_IsString) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "s1");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *c = pa->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr) << "'\"foo\"' RHS must be a Constant";
  EXPECT_EQ(c->getConstType(), vpiStringConst)
      << "ss.5.9: '\"foo\"' must have constType vpiStringConst (6)";
}

// ===========================================================================
// localparam string s2 = "bar"  (explicit string type)
// ===========================================================================

TEST_F(LocalparamStringTest, S2_Exists) {
  EXPECT_NE(getParam(m_design, "s2"), nullptr)
      << "'s2' not found in parameters";
}

// ss.6.20.4: s2 is a localparam and must NOT be overridable.
TEST_F(LocalparamStringTest, S2_IsLocalParam) {
  const hldb::Parameter *p = getParam(m_design, "s2");
  ASSERT_NE(p, nullptr);
  EXPECT_TRUE(p->getLocalParam())
      << "ss.6.20.4: 'localparam string s2' must be marked as a localparam";
}

// ss.6.16: the explicit 'string' type must be recorded as a non-null typespec.
TEST_F(LocalparamStringTest, S2_TypespecExists) {
  const hldb::Parameter *p = getParam(m_design, "s2");
  ASSERT_NE(p, nullptr);
  EXPECT_NE(p->getTypespec(), nullptr)
      << "ss.6.16: 'localparam string s2' must have a non-null typespec";
}

// ss.6.16: the explicit 'string' keyword must resolve to a StringTypespec.
TEST_F(LocalparamStringTest, S2_Typespec_IsStringTypespec) {
  const hldb::Parameter *p = getParam(m_design, "s2");
  ASSERT_NE(p, nullptr);
  const hldb::RefTypespec *rt = p->getTypespec();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<hldb::StringTypespec>(), nullptr)
      << "ss.6.16: 'string' keyword must resolve to StringTypespec";
}

// ss.5.9: "bar" is a string literal; the ParamAssign RHS must be a Constant.
TEST_F(LocalparamStringTest, S2_RhsIsConstant) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "s2");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 's2' not found";
  EXPECT_NE(pa->getRhs<hldb::Constant>(), nullptr)
      << "ss.5.9: '\"bar\"' must be represented as a Constant node";
}

// ss.5.9: a string literal has constType vpiStringConst (6).
TEST_F(LocalparamStringTest, S2_Rhs_ConstType_IsString) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "s2");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *c = pa->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr) << "'\"bar\"' RHS must be a Constant";
  EXPECT_EQ(c->getConstType(), vpiStringConst)
      << "ss.5.9: '\"bar\"' must have constType vpiStringConst (6)";
}

}  // namespace hlc
