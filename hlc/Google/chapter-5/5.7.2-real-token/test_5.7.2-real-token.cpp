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

// Spec-based validation of the 'real' keyword per IEEE 1800-2017 §5.7.2.
//
// §5.7.2 rule under test:
//   "The default type for fixed-point format (e.g., 1.2) and exponent format
//    (e.g., 2.0e10) shall be real."
//
// The 'real' keyword declares an IEEE 754 double-precision scalar variable.
// In UHDM, net 'a' must carry a RealTypespec — not a LogicTypespec (used for
// 'logic') or IntegerTypespec (used for the 'integer' keyword).
//
// SV source:
//   module top();
//     real a;
//   endmodule
//
// UHDM:
//   Module work@top
//     Net a → RefTypespec → RealTypespec
//   RealTypespec has no packed dimension ranges (real is a scalar type).

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/real_typespec.h>
#include <hldb/ref_typespec.h>

namespace hlc {

class RealToken : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "5.7.2-real-token.hlc"});

    ASSERT_NE(m_session, nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design, nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }
};

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("work@top", d->getAllModules());
}

static const hldb::Net *getNetA(const hldb::Design *d) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getNets()) return nullptr;
  return hldb::findByName<hldb::Net>("a", m->getNets());
}

// ---------------------------------------------------------------------------
// Module structure
// ---------------------------------------------------------------------------
TEST_F(RealToken, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr) << "module 'work@top' not found";
}

TEST_F(RealToken, OneNetExists) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getNets(), nullptr);
  EXPECT_EQ(m->getNets()->size(), 1u) << "expected 1 net: a";
}

// ---------------------------------------------------------------------------
// §5.7.2: the 'real' keyword must produce a RealTypespec in UHDM.
// A LogicTypespec or IntegerTypespec would indicate Surelog misidentified
// the type.
// ---------------------------------------------------------------------------
TEST_F(RealToken, NetA_HasRealTypespec) {
  const hldb::Net *const net = getNetA(m_design);
  ASSERT_NE(net, nullptr);
  ASSERT_NE(net->getTypespec(), nullptr) << "net 'a' has no typespec";
  EXPECT_NE(net->getTypespec()->getActual<hldb::RealTypespec>(), nullptr)
      << "§5.7.2: 'real a' must produce a RealTypespec, not LogicTypespec "
         "or IntegerTypespec";
}

// ---------------------------------------------------------------------------
// §5.7.2: 'real' is IEEE 754 double-precision — a scalar type with no packed
// dimension. Surelog must not attach any typespec other than RealTypespec.
// ---------------------------------------------------------------------------
TEST_F(RealToken, NetA_TypespecIsNotLogic) {
  const hldb::Net *const net = getNetA(m_design);
  ASSERT_NE(net, nullptr);
  ASSERT_NE(net->getTypespec(), nullptr);
  EXPECT_EQ(net->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr)
      << "§5.7.2: 'real' must not be represented as LogicTypespec";
}

// ---------------------------------------------------------------------------
// No initial block — the module only declares 'real a' with no assignments.
// ---------------------------------------------------------------------------
TEST_F(RealToken, NoProcesses) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getProcesses() || m->getProcesses()->empty())
      << "module has no initial or always blocks";
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
