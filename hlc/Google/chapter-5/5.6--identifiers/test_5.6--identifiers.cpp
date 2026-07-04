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

// Validates that all legal SV identifier forms are accepted and appear in UHDM:
//   reg shiftreg_a;      — underscore in middle
//   reg busa_index;      — underscore in middle
//   reg error_condition; — underscore in middle
//   reg merge_ab;        — underscore in middle
//   reg _bus3;           — leading underscore
//   reg n$657;           — dollar sign in identifier
//   reg sensitive;       — lowercase
//   reg Sensitive;       — uppercase start (case-distinct from 'sensitive')
//
// UHDM: Module name:work@identifiers with 8 Net nodes, all LogicTypespec.
// Case sensitivity: 'sensitive' and 'Sensitive' are distinct nets.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>

namespace hlc {

class Identifiers : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.6--identifiers.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("work@identifiers", d->getAllModules());
}

static bool hasNet(const hldb::Module *m, std::string_view name) {
  if (!m->getNets()) return false;
  for (const hldb::Net *const n : *m->getNets())
    if (n->getName() == name) return true;
  return false;
}

// ---------------------------------------------------------------------------
// Module
// ---------------------------------------------------------------------------
TEST_F(Identifiers, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'work@identifiers' not found"; }

TEST_F(Identifiers, EightNetsExist) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getNets(), nullptr);
  EXPECT_EQ(m->getNets()->size(), 8u);
}

// ---------------------------------------------------------------------------
// Standard identifiers with underscores
// ---------------------------------------------------------------------------
TEST_F(Identifiers, NetShiftregA) {
  EXPECT_TRUE(hasNet(getTop(m_design), "shiftreg_a")) << "net 'shiftreg_a' not found";
}

TEST_F(Identifiers, NetBusaIndex) {
  EXPECT_TRUE(hasNet(getTop(m_design), "busa_index")) << "net 'busa_index' not found";
}

TEST_F(Identifiers, NetErrorCondition) {
  EXPECT_TRUE(hasNet(getTop(m_design), "error_condition")) << "net 'error_condition' not found";
}

TEST_F(Identifiers, NetMergeAb) { EXPECT_TRUE(hasNet(getTop(m_design), "merge_ab")) << "net 'merge_ab' not found"; }

// ---------------------------------------------------------------------------
// Leading-underscore identifier
// ---------------------------------------------------------------------------
TEST_F(Identifiers, NetBus3WithLeadingUnderscore) {
  EXPECT_TRUE(hasNet(getTop(m_design), "_bus3")) << "net '_bus3' (leading underscore) not found";
}

// ---------------------------------------------------------------------------
// Dollar-sign identifier
// ---------------------------------------------------------------------------
TEST_F(Identifiers, NetN657WithDollarSign) {
  EXPECT_TRUE(hasNet(getTop(m_design), "n$657")) << "net 'n$657' (dollar sign) not found";
}

// ---------------------------------------------------------------------------
// Case sensitivity: 'sensitive' and 'Sensitive' are distinct nets
// ---------------------------------------------------------------------------
TEST_F(Identifiers, NetSensitiveLowercase) {
  EXPECT_TRUE(hasNet(getTop(m_design), "sensitive")) << "net 'sensitive' (lowercase) not found";
}

TEST_F(Identifiers, NetSensitiveUppercase) {
  EXPECT_TRUE(hasNet(getTop(m_design), "Sensitive")) << "net 'Sensitive' (uppercase) not found";
}

TEST_F(Identifiers, SensitiveAndSensitiveAreDistinct) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getNets(), nullptr);

  const hldb::Net *lower = nullptr;
  const hldb::Net *upper = nullptr;
  for (const hldb::Net *const n : *m->getNets()) {
    if (n->getName() == "sensitive") lower = n;
    if (n->getName() == "Sensitive") upper = n;
  }
  ASSERT_NE(lower, nullptr) << "'sensitive' not found";
  ASSERT_NE(upper, nullptr) << "'Sensitive' not found";
  EXPECT_NE(lower, upper) << "'sensitive' and 'Sensitive' must be distinct nets";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
