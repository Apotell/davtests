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

// Validates that the 'integer' variable type is parsed and represented in UHDM
// with an IntegerTypespec marked as signed.
//
// SV source:
//   module top();
//     integer a;
//   endmodule
//
// The SV 'integer' keyword declares a 32-bit signed 2's-complement variable.
// In UHDM the net 'a' carries a RefTypespec whose actual typespec is an
// IntegerTypespec with vpiSigned: true.  This is distinct from 'logic' which
// produces a LogicTypespec, and from 'int' (SystemVerilog 2-state type).

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/integer_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_typespec.h>

namespace hlc {

class IntegersToken : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.7.1--integers-token.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("work@top", d->getAllModules());
}

// ---------------------------------------------------------------------------
// Module structure
// ---------------------------------------------------------------------------
TEST_F(IntegersToken, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'work@top' not found"; }

TEST_F(IntegersToken, OneNetExists) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getNets(), nullptr);
  EXPECT_EQ(m->getNets()->size(), 1u) << "expected exactly 1 net ('a')";
}

TEST_F(IntegersToken, NetIsNamedA) {
  const hldb::Net *const net = hldb::findByName<hldb::Net>("a", getTop(m_design)->getNets());
  EXPECT_NE(net, nullptr) << "net 'a' not found";
}

TEST_F(IntegersToken, NoProcesses) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getProcesses() || m->getProcesses()->empty()) << "module should have no initial/always processes";
}

// ---------------------------------------------------------------------------
// 'integer a' → Net typespec is IntegerTypespec, marked signed.
// The typespec chain: Net::getTypespec() → RefTypespec → getActual<IntegerTypespec>()
// ---------------------------------------------------------------------------
TEST_F(IntegersToken, NetTypespecIsInteger) {
  const hldb::Net *const net = hldb::findByName<hldb::Net>("a", getTop(m_design)->getNets());
  ASSERT_NE(net, nullptr);
  const hldb::RefTypespec *const ref = net->getTypespec();
  ASSERT_NE(ref, nullptr) << "net 'a' has no typespec";
  const auto *const intTs = ref->getActual<hldb::IntegerTypespec>();
  EXPECT_NE(intTs, nullptr) << "'integer a' should have an IntegerTypespec, not LogicTypespec or other";
}

TEST_F(IntegersToken, IntegerTypespecIsSigned) {
  const hldb::Net *const net = hldb::findByName<hldb::Net>("a", getTop(m_design)->getNets());
  ASSERT_NE(net, nullptr);
  const hldb::RefTypespec *const ref = net->getTypespec();
  ASSERT_NE(ref, nullptr);
  const auto *const intTs = ref->getActual<hldb::IntegerTypespec>();
  ASSERT_NE(intTs, nullptr);
  EXPECT_TRUE(intTs->getSigned()) << "SV 'integer' is a signed 32-bit type; vpiSigned should be true";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
