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

// Validates that all legal escaped-identifier forms are accepted and appear
// in UHDM.  In SV, an escaped identifier begins with '\' and ends at the
// first whitespace; the backslash is NOT part of the identifier name.
//
//   SV source           → UHDM net name
//   \busa+index         → "busa+index"
//   \-clock             → "-clock"
//   \***error-condition*** → "***error-condition***"
//   \net1/\net2         → "net1/\net2"
//   \{a,b}              → "{a,b}"
//   \a*(b+c)            → "a*(b+c)"
//
// UHDM: Module name:work@identifiers with 6 Net nodes, all LogicTypespec.
// No syntax errors — escaped identifiers may contain any printable character.

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/design.h>
#include <uhdm/module.h>
#include <uhdm/net.h>

namespace SURELOG {

class EscapedIdentifiers : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "5.6.1--escaped-identifiers.hlc"});

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

static const uhdm::Module *getTop(const uhdm::Design *d) {
  return uhdm::findByName<uhdm::Module>("work@identifiers", d->getAllModules());
}

static bool hasNet(const uhdm::Module *m, std::string_view name) {
  if (!m->getNets()) return false;
  for (const uhdm::Net *const n : *m->getNets())
    if (n->getName() == name) return true;
  return false;
}

// ---------------------------------------------------------------------------
// Module
// ---------------------------------------------------------------------------
TEST_F(EscapedIdentifiers, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr) << "module 'work@identifiers' not found";
}

TEST_F(EscapedIdentifiers, SixNetsExist) {
  const uhdm::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getNets(), nullptr);
  EXPECT_EQ(m->getNets()->size(), 6u);
}

// ---------------------------------------------------------------------------
// Each escaped identifier stored without the leading backslash
// ---------------------------------------------------------------------------
TEST_F(EscapedIdentifiers, NetBusaPlusIndex) {
  // \busa+index  →  "busa+index"
  EXPECT_TRUE(hasNet(getTop(m_design), "busa+index"))
      << "net 'busa+index' not found";
}

TEST_F(EscapedIdentifiers, NetMinusClock) {
  // \-clock  →  "-clock"
  EXPECT_TRUE(hasNet(getTop(m_design), "-clock"))
      << "net '-clock' not found";
}

TEST_F(EscapedIdentifiers, NetErrorConditionWithStars) {
  // \***error-condition***  →  "***error-condition***"
  EXPECT_TRUE(hasNet(getTop(m_design), "***error-condition***"))
      << "net '***error-condition***' not found";
}

TEST_F(EscapedIdentifiers, NetNet1SlashNet2) {
  // \net1/\net2  →  "net1/\net2"
  EXPECT_TRUE(hasNet(getTop(m_design), "net1/\\net2"))
      << "net 'net1/\\net2' not found";
}

TEST_F(EscapedIdentifiers, NetCurlyAB) {
  // \{a,b}  →  "{a,b}"
  EXPECT_TRUE(hasNet(getTop(m_design), "{a,b}"))
      << "net '{a,b}' not found";
}

TEST_F(EscapedIdentifiers, NetAStarBPlusC) {
  // \a*(b+c)  →  "a*(b+c)"
  EXPECT_TRUE(hasNet(getTop(m_design), "a*(b+c)"))
      << "net 'a*(b+c)' not found";
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
