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

// Tests for tests/LowMemLib (tags: LowMemLib)
//   command line: -y mylib mytop.v +libext+.sv+.v
//
//   mytop.v:
//     module mytop();
//     endmodule
//
//   mylib/mylib.sv (library directory searched via "-y mylib"):
//     module mylib();
//     endmodule
//
// The construct under test is library-directory searching: "mytop.v" is the
// only source file given on the command line; "mylib" is a library
// directory whose files ("+libext+.sv+.v") are only consulted to resolve
// otherwise undefined module references (the same mechanism exercised by
// hlc/DashYTest, where an instantiated module is pulled in from "-y lib").
//
// What is checked (IEEE 1800-2023):
//   - module mytop exists and is empty (no ports, nets, variables,
//     processes, instances), matching "module mytop(); endmodule".
//   - 23.3.1: mytop appears in no instantiation statement, so it is a
//     top-level module.
//   - Clause 33 library binding (33.3/33.4: a library holds cells that are
//     bound to instantiations during elaboration): mytop instantiates
//     nothing, so no cell is needed from the library directory, and module
//     "mylib" from mylib/mylib.sv must NOT be loaded into the design.
//   - no COMP_FAILED_TO_BIND diagnostics (there are no references to bind).
//
// What is NOT checked and why:
//   - the exact semantics of the "-y"/"+libext+" options themselves: these
//     are tool command-line conventions, not defined by IEEE 1800; only the
//     standard library-binding consequence (unreferenced library cells are
//     not part of the design) is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>

namespace hlc {

class LowMemLibTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "LowMemLib.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }
};

TEST_F(LowMemLibTest, ModuleMytopExists) { EXPECT_NE(getModule("mytop"), nullptr); }

TEST_F(LowMemLibTest, MytopIsEmpty) {
  const hldb::Module *const m = getModule("mytop");
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getDefName(), "mytop");
  EXPECT_TRUE(m->getPorts() == nullptr || m->getPorts()->empty());
  EXPECT_TRUE(m->getNets() == nullptr || m->getNets()->empty());
  EXPECT_TRUE(m->getVariables() == nullptr || m->getVariables()->empty());
  EXPECT_TRUE(m->getProcesses() == nullptr || m->getProcesses()->empty());
  EXPECT_TRUE(m->getRefInstances() == nullptr || m->getRefInstances()->empty());
}

// 23.3.1
TEST_F(LowMemLibTest, MytopIsTopLevelModule) {
  const hldb::Module *const m = getModule("mytop");
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(m->getTopModule()) << "23.3.1: mytop appears in no instantiation statement";
}

// Clause 33: unreferenced library cells are not part of the design.
TEST_F(LowMemLibTest, UnreferencedLibraryModuleIsNotLoaded) {
  EXPECT_EQ(getModule("mylib"), nullptr)
      << "nothing instantiates 'mylib', so the library directory must not contribute it to the design";
}

TEST_F(LowMemLibTest, OnlyMytopIsInTheDesign) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  EXPECT_EQ(m_design->getAllModules()->size(), 1u);
}

TEST_F(LowMemLibTest, NoBindingFailures) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "mylib"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "mytop"), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
