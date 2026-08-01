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

// Spec-based validation of the 'real' keyword per IEEE 1800-2017 Sec 5.7.2.
//
// Sec 5.7.2 rule under test:
//   "The default type for fixed-point format (e.g., 1.2) and exponent format
//    (e.g., 2.0e10) shall be real."
//
// The 'real' keyword declares an IEEE 754 double-precision scalar variable.
// In UHDM, variable 'a' must carry a RealTypespec -- not a LogicTypespec (used for
// 'logic') or IntegerTypespec (used for the 'integer' keyword).
//
// SV source:
//   module top();
//     real a;
//   endmodule
//
// UHDM:
//   Module top
//     Variable a -> RefTypespec -> RealTypespec
//   RealTypespec has no packed dimension ranges (real is a scalar type).

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/real_typespec.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>

namespace hlc {

class RealToken : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.7.2-real-token.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("top", d->getAllModules());
}

static const hldb::Variable *getVariableA(const hldb::Design *d) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getVariables()) return nullptr;
  return hldb::findByName<hldb::Variable>("a", m->getVariables());
}

// ----
// Module structure
// ----
TEST_F(RealToken, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'top' not found"; }

TEST_F(RealToken, OneVariableExists) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getVariables(), nullptr);
  EXPECT_EQ(m->getVariables()->size(), 1u) << "expected 1 variable: a";
}

// `real` is a variable keyword, not one of the net-type keywords in IEEE
// 1800-2023 Sec 6.7/6.8, so 'a' must not appear as a Net.
TEST_F(RealToken, ModuleHasNoNets) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getNets() || m->getNets()->empty()) << "'real a' must not appear as a Net";
}

// ----
// Sec 5.7.2: the 'real' keyword must produce a RealTypespec in UHDM.
// A LogicTypespec or IntegerTypespec would indicate Surelog misidentified
// the type.
// ----
TEST_F(RealToken, VariableA_HasRealTypespec) {
  const hldb::Variable *const var = getVariableA(m_design);
  ASSERT_NE(var, nullptr);
  ASSERT_NE(var->getTypespec(), nullptr) << "variable 'a' has no typespec";
  EXPECT_NE(var->getTypespec()->getActual<hldb::RealTypespec>(), nullptr)
      << "Sec 5.7.2: 'real a' must produce a RealTypespec, not LogicTypespec "
         "or IntegerTypespec";
}

// ----
// Sec 5.7.2: 'real' is IEEE 754 double-precision -- a scalar type with no packed
// dimension. Surelog must not attach any typespec other than RealTypespec.
// ----
TEST_F(RealToken, VariableA_TypespecIsNotLogic) {
  const hldb::Variable *const var = getVariableA(m_design);
  ASSERT_NE(var, nullptr);
  ASSERT_NE(var->getTypespec(), nullptr);
  EXPECT_EQ(var->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr)
      << "Sec 5.7.2: 'real' must not be represented as LogicTypespec";
}

// ----
// No initial block -- the module only declares 'real a' with no assignments.
// ----
TEST_F(RealToken, NoProcesses) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getProcesses() || m->getProcesses()->empty()) << "module has no initial or always blocks";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
