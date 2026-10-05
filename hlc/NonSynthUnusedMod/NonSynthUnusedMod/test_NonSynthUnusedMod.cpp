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

// Tests for tests/NonSynthUnusedMod (compiled with HLC's -synth option and
// library directory "-y . +libext+.v"; only dut.sv is given explicitly):
//
//   dut.sv:       module dut();
//                    top top();
//                 endmodule
//   top.v:        module top();
//                 endmodule
//   nonsynth.v:   module nonsynth();
//                    initial
//                      #1 a = b;
//                 endmodule
//
// What to check and why (IEEE 1800-2023):
//   - Sec 23.3.2: "top top();" is a module instantiation of module top
//     with instance name top inside dut; the reference must bind to the
//     module top definition (located through the library directory, Sec
//     33.5.2 "only compiling the source descriptions necessary to bind the
//     design").
//   - Sec 23.3.1: top-level modules "do not appear in any module
//     instantiation statement". dut is not instantiated; top is
//     instantiated by dut. nonsynth is not referenced by anything.
//   - Sec 3.13(a) / 23.3.2: the instance name 'top' may equal the module
//     name 'top' -- module names live in the definitions name space and
//     instance names in the module's local name space, so no error.
//   - nonsynth.v contains "#1 a = b;" with undeclared a/b (illegal per Sec
//     6.5 if it were part of the design). Since nothing instantiates
//     nonsynth, no diagnostic may be produced for those identifiers.
//
// What is NOT checked and why:
//   - Whether nonsynth.v is parsed at all and whether it appears in
//     getAllModules(): -y library scanning is a tool convention, not
//     defined by IEEE 1800-2023.
//   - -synth non-synthesizable reporting: synthesizable subset is a tool
//     policy, not part of the standard.
//   - getTopModules(): the .hlc does not request elaboration.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class NonSynthUnusedModTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "NonSynthUnusedMod.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::RefInstance *getTopInstance() {
    const hldb::Module *const dut = getModule("dut");
    if (dut == nullptr || dut->getRefInstances() == nullptr) return nullptr;
    return hldb::findByName<hldb::RefInstance>("top", dut->getRefInstances());
  }

  // Returns true if any module in the design instantiates definition 'defName'.
  static bool isInstantiated(std::string_view defName) {
    if (m_design->getAllModules() == nullptr) return false;
    for (const hldb::Module *const m : *m_design->getAllModules()) {
      if (m->getRefInstances() == nullptr) continue;
      for (const hldb::RefInstance *const ri : *m->getRefInstances()) {
        if (ri->getTypespec() != nullptr && ri->getTypespec()->getName() == defName) return true;
      }
    }
    return false;
  }
};

TEST_F(NonSynthUnusedModTest, ModuleDutExists) { ASSERT_NE(getModule("dut"), nullptr); }

TEST_F(NonSynthUnusedModTest, ModuleTopLoadedFromLibrary) {
  EXPECT_NE(getModule("top"), nullptr) << "module 'top' (top.v) must be found to bind 'top top();'";
}

TEST_F(NonSynthUnusedModTest, DutHasSingleInstanceNamedTop) {
  const hldb::Module *const dut = getModule("dut");
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getRefInstances(), nullptr);
  ASSERT_EQ(dut->getRefInstances()->size(), 1u);
  EXPECT_EQ(dut->getRefInstances()->at(0)->getName(), std::string_view("top"));
}

TEST_F(NonSynthUnusedModTest, InstanceTopBindsToModuleTop) {
  const hldb::RefInstance *const ri = getTopInstance();
  ASSERT_NE(ri, nullptr);
  ASSERT_NE(ri->getTypespec(), nullptr);
  EXPECT_EQ(ri->getTypespec()->getName(), std::string_view("top"));
  const hldb::ModuleTypespec *const mts = ri->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mts, nullptr) << "Sec 23.3.2: module_identifier must resolve to a module definition";
  ASSERT_NE(mts->getModule(), nullptr);
  EXPECT_EQ(mts->getModule(), getModule("top"));
}

TEST_F(NonSynthUnusedModTest, InstantiationRelationships) {
  EXPECT_FALSE(isInstantiated("dut")) << "Sec 23.3.1: dut is a top-level module";
  EXPECT_TRUE(isInstantiated("top")) << "top is instantiated by dut";
  EXPECT_FALSE(isInstantiated("nonsynth")) << "nothing instantiates nonsynth";
}

TEST_F(NonSynthUnusedModTest, NoUndefinedModuleDiagnostics) {
  EXPECT_EQ(findError(ErrorDefinition::ELAB_NO_MODULE_DEFINITION, "top"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "top"), nullptr);
}

TEST_F(NonSynthUnusedModTest, NoDiagnosticsFromUnusedLibraryModule) {
  // nonsynth is not part of the design, so its undeclared a/b must not be
  // reported.
  for (const char *const sym : {"a", "b"}) {
    EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, sym), nullptr) << sym;
    EXPECT_EQ(findError(ErrorDefinition::COMP_UNDEFINED_VARIABLE, sym), nullptr) << sym;
    EXPECT_EQ(findError(ErrorDefinition::HLDB_UNDEFINED_VARIABLE, sym), nullptr) << sym;
  }
}

TEST_F(NonSynthUnusedModTest, NoSyntaxOrFatalErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbFatal, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
