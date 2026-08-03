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

// Tests for 6.6.7--nettype.sv (tags: 6.6.7)
//   module top();
//     nettype real real_net;
//   endmodule
//
// What to check and why (IEEE 1800-2023 6.6.7 "User-defined nettypes",
// p.97-98, checked before any test code was written):
//   "A user-defined nettype allows users to describe more general
//   abstract values for a wire ... This nettype is similar to a typedef
//   (see 6.18) in some ways, but shall only be used in declaring a net.
//   It provides a name for a particular data type and optionally an
//   associated resolution function." "A real or shortreal type" is
//   explicitly listed as a valid nettype base data type, so this
//   declaration is fully legal with no expected errors.
//
//   hldb has no dedicated "user-defined nettype" class -- it reuses
//   TypedefTypespec (which carries both a getTypedefAlias() base type
//   and a getResolutionFunc() slot), matching the spec's own "similar to
//   a typedef" description. This is NOT a misclassification: unlike the
//   6.5-series net/variable bug, there is no net or variable *instance*
//   here at all -- "nettype real real_net;" only introduces a new named
//   type, it does not declare a net (spec: "shall only be used in
//   declaring a net" -- i.e. by something else, later, which this file
//   does not do).
//
// What is checked:
//   - module top exists, has exactly 1 typespec: TypedefTypespec named
//     "real_net"
//   - its alias (getTypedefAlias()) resolves to RealTypespec (from
//     "real")
//   - its resolution function (getResolutionFunc()) is null (no 'with'
//     clause present)
//   - top has no Nets (the nettype declaration itself does not
//     instantiate a net), no processes, no task/functions
//   - compiler reports zero errors (this file is fully legal per 6.6.7)
//
// What is NOT checked and why:
//   - none: every corner above is fully structural and checkable without
//     simulation.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/real_typespec.h>
#include <hldb/ref_typespec.h>
#include <hldb/typedef_typespec.h>

namespace hlc {

class NettypeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.6.7--nettype.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

TEST_F(NettypeTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

// ---------------------------------------------------------------------------
// Module has exactly one typespec: TypedefTypespec "real_net"
// ---------------------------------------------------------------------------
TEST_F(NettypeTest, ModuleHasOneTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);
  EXPECT_EQ(top->getTypespecs()->size(), 1u);
}

TEST_F(NettypeTest, NettypeIsTypedefTypespecNamedRealNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);
  const hldb::TypedefTypespec *const td = any_cast<hldb::TypedefTypespec>(top->getTypespecs()->at(0));
  ASSERT_NE(td, nullptr) << "nettype declaration creates a TypedefTypespec";
  EXPECT_EQ(td->getName(), "real_net");
}

// ---------------------------------------------------------------------------
// TypedefTypespec alias: RefTypespec -> RealTypespec
// ---------------------------------------------------------------------------
TEST_F(NettypeTest, NettypeAliasIsRealTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const td = any_cast<hldb::TypedefTypespec>(top->getTypespecs()->at(0));
  ASSERT_NE(td, nullptr);
  const hldb::RefTypespec *const alias = td->getTypedefAlias();
  ASSERT_NE(alias, nullptr);
  EXPECT_NE(alias->getActual<hldb::RealTypespec>(), nullptr)
      << "nettype real real_net: alias base type is RealTypespec, a spec-listed valid nettype "
         "base type (IEEE 1800-2023 6.6.7 item c)";
}

// ---------------------------------------------------------------------------
// No resolution function -- this nettype has no 'with' clause
// ---------------------------------------------------------------------------
TEST_F(NettypeTest, NettypeHasNoResolutionFunction) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const td = any_cast<hldb::TypedefTypespec>(top->getTypespecs()->at(0));
  ASSERT_NE(td, nullptr);
  EXPECT_EQ(td->getResolutionFunc(), nullptr) << "nettype without 'with' clause has no resolution function";
}

// ---------------------------------------------------------------------------
// No nets or processes -- nettype declaration does not instantiate a net
// ---------------------------------------------------------------------------
TEST_F(NettypeTest, NoNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "IEEE 1800-2023 6.6.7: the nettype declaration only names a type; it 'shall only be "
         "used in declaring a net' elsewhere, which this file does not do";
}

TEST_F(NettypeTest, NoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

TEST_F(NettypeTest, NoTaskFunctions) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getTaskFuncs() == nullptr || top->getTaskFuncs()->empty())
      << "nettype without resolution function has no task/function declarations";
}

TEST_F(NettypeTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
