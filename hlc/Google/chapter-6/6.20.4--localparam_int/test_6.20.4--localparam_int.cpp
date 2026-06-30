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

// Spec-based validation of IEEE 1800-2017 ss.6.20.4 local parameter with
// an explicit 'int' type.
// SV: tests/Google/chapter-6/6.20.4--localparam_int.sv
//
//   module top();
//       localparam int p = 123;
//   endmodule
//
// -- ss.6.20.4 + ss.6.11.2 rules under test -----------------------------------
//
// Local parameter (ss.6.20.4):
//   * 'localparam' declares a constant that cannot be changed at instantiation.
//   * UHDM represents it as a Parameter node with getLocalParam() == true.
//   * The default value is stored in ParamAssign::getRhs(), not getExpr().
//
// Explicit 'int' type (ss.6.11.2):
//   * 'int' is a 2-state signed 32-bit integer data type.
//   * It is distinct from 'integer' (which is 4-state).
//   * Declaring 'localparam int p' attaches an IntTypespec to the parameter.
//   * 'int' is a signed type; IntTypespec::getSigned() must return true.
//
// -- UHDM tree ----------------------------------------------------------------
//
//   Module name:work@top
//   +-- getParameters() (AnyCollection, 1 item)
//   |   +-- [0] Parameter name:"p"  localParam: true
//   |           typespec: RefTypespec -> IntTypespec { signed: true }
//   +-- getParamAssigns() (ParamAssignCollection, 1 item)
//       +-- [0] ParamAssign
//               lhs: RefObj name:"p"  actual: Parameter name:"p"
//               rhs: Constant { decompile: "123" }

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_typespec.h>

#include <string>

namespace hlc {

class LocalparamIntTest : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.20.4--localparam_int.hlc"});

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

TEST_F(LocalparamIntTest, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr) << "module 'work@top' not found";
}

// ===========================================================================
// Parameter collection
// ===========================================================================

TEST_F(LocalparamIntTest, ParameterCollectionExists) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(m->getParameters(), nullptr)
      << "module 'top' must have a parameter collection";
}

// ss.6.20.4: 'localparam int p = 123' declares exactly one local parameter.
TEST_F(LocalparamIntTest, ParameterCount) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParameters(), nullptr);
  EXPECT_EQ(m->getParameters()->size(), 1u)
      << "module 'top' declares exactly one localparam: p";
}

// ===========================================================================
// localparam int p = 123
// ===========================================================================

// ss.6.20.4: 'localparam' must produce a Parameter node named "p".
TEST_F(LocalparamIntTest, P_Exists) {
  EXPECT_NE(getParam(m_design, "p"), nullptr)
      << "'p' not found in parameters";
}

// ss.6.20.4: a localparam is NOT overridable at instantiation;
// getLocalParam() must return true.
TEST_F(LocalparamIntTest, P_IsLocalParam) {
  const hldb::Parameter *p = getParam(m_design, "p");
  ASSERT_NE(p, nullptr);
  EXPECT_TRUE(p->getLocalParam())
      << "ss.6.20.4: 'localparam int p' must be marked as a localparam";
}

// ss.6.11.2: the explicit 'int' type must be recorded as a non-null typespec.
TEST_F(LocalparamIntTest, P_TypespecExists) {
  const hldb::Parameter *p = getParam(m_design, "p");
  ASSERT_NE(p, nullptr);
  EXPECT_NE(p->getTypespec(), nullptr)
      << "ss.6.11.2: 'localparam int p' must have a non-null typespec";
}

// ss.6.11.2: the explicit 'int' type must resolve to an IntTypespec.
TEST_F(LocalparamIntTest, P_Typespec_IsIntTypespec) {
  const hldb::Parameter *p = getParam(m_design, "p");
  ASSERT_NE(p, nullptr);
  const hldb::RefTypespec *rt = p->getTypespec();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<hldb::IntTypespec>(), nullptr)
      << "ss.6.11.2: 'int' must resolve to IntTypespec (32-bit 2-state)";
}

// ss.6.11.2: 'int' is a signed type; the IntTypespec must report signed.
TEST_F(LocalparamIntTest, P_Typespec_IsSigned) {
  const hldb::Parameter *p = getParam(m_design, "p");
  ASSERT_NE(p, nullptr);
  const hldb::RefTypespec *rt = p->getTypespec();
  ASSERT_NE(rt, nullptr);
  const hldb::IntTypespec *ts = rt->getActual<hldb::IntTypespec>();
  ASSERT_NE(ts, nullptr);
  EXPECT_TRUE(ts->getSigned())
      << "ss.6.11.2: 'int' is a signed type";
}

// ss.6.20.4: the value '123' is a constant_expression stored in the
// ParamAssign RHS, not in Parameter::getExpr().
TEST_F(LocalparamIntTest, P_RhsIsConstant) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "p");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'p' not found";
  EXPECT_NE(pa->getRhs<hldb::Constant>(), nullptr)
      << "'localparam int p = 123': RHS must be a Constant";
}

// ss.6.20.4: the declared value is 123; the Constant must decompile to "123".
TEST_F(LocalparamIntTest, P_RhsDecompile) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "p");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'p' not found";
  const hldb::Constant *c = pa->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr) << "'localparam int p = 123': RHS must be a Constant";
  EXPECT_EQ(std::string(c->getDecompile()), "123")
      << "'localparam int p = 123': decompile must be \"123\"";
}

}  // namespace hlc
