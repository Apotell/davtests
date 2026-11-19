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

// Tests for tests/Reachability/dut.sv -- see that file for the source.
//
// Top-level modules and programs (IEEE 1800-2023 Sec 23.3.1, 24.3) are the modules and programs,
// not nested in another module, that appear in no instantiation anywhere. Reachable is what the
// design rooted at them uses: what it instantiates (including through bind, Sec 23.11, and
// implicitly instantiated nested modules, Sec 23.4), the classes it uses as types, and the packages
// and classes owning what it calls. HLC binds everything; reachability decides what is reported.

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/program.h>
#include <hldb/ref_typespec.h>
#include <hldb/unit.h>
#include <hldb/variable.h>

#include <gtest/gtest.h>

#include <string_view>

namespace hlc {

class ReachabilityTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Reachability.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }
  static const hldb::Module *getNestedModule(std::string_view name) {
    const hldb::Module *const top = getModule("top");
    return (top == nullptr) ? nullptr : hldb::findByDefName<hldb::Module>(name, top->getModules());
  }
  static const hldb::Program *getProgram(std::string_view name) {
    return hldb::findByDefName<hldb::Program>(name, m_design->getAllPrograms());
  }
  static const hldb::ClassDefn *getClass(std::string_view name) {
    return (m_unit == nullptr) ? nullptr : hldb::findByDefName<hldb::ClassDefn>(name, m_unit->getClassDefns());
  }
  static const hldb::Package *getPackage(std::string_view name) {
    return hldb::findByName<hldb::Package>(name, m_design->getAllPackages());
  }

  // The class a variable of `top` is declared with (after specialization).
  static const hldb::ClassDefn *getVariableClass(std::string_view variableName) {
    const hldb::Module *const top = getModule("top");
    if (top == nullptr) return nullptr;
    const hldb::Variable *const v = hldb::findByName<hldb::Variable>(variableName, top->getVariables());
    if ((v == nullptr) || (v->getTypespec() == nullptr)) return nullptr;
    const hldb::ClassTypespec *const ct = v->getTypespec<hldb::RefTypespec>()->getActual<hldb::ClassTypespec>();
    return (ct == nullptr) ? nullptr : ct->getClassDefn();
  }
};

// ---- Top-level modules and programs (Sec 23.3.1, 24.3) ----

TEST_F(ReachabilityTest, TopIsTopLevel) {
  ASSERT_NE(getModule("top"), nullptr);
  EXPECT_TRUE(isTopLevel(getModule("top")));
}

TEST_F(ReachabilityTest, TopAndProgAreTheOnlyTopLevels) { EXPECT_EQ(getTopLevelCount(), 2u); }

TEST_F(ReachabilityTest, UninstantiatedProgramIsTopLevel) {
  ASSERT_NE(getProgram("prog"), nullptr);
  EXPECT_TRUE(isTopLevel(getProgram("prog")));
}

TEST_F(ReachabilityTest, InstantiatedModulesAreNotTopLevel) {
  for (const std::string_view name : {"leaf", "gen_only", "orphan_leaf", "bound_mod"}) {
    const hldb::Module *const m = getModule(name);
    ASSERT_NE(m, nullptr) << name;
    EXPECT_FALSE(isTopLevel(m)) << name;
  }
}

TEST_F(ReachabilityTest, NestedModulesAreNotTopLevel) {
  for (const std::string_view name : {"nested_implicit", "nested_ignored"}) {
    const hldb::Module *const m = getNestedModule(name);
    ASSERT_NE(m, nullptr) << name;
    EXPECT_FALSE(isTopLevel(m)) << name;
  }
}

TEST_F(ReachabilityTest, TopLevelsAreReported) {
  EXPECT_NE(findEvent(EventType::ElabTopLevelModule, "top"), nullptr);
  EXPECT_NE(findEvent(EventType::ElabTopLevelModule, "prog"), nullptr);
  EXPECT_NE(findEvent(EventType::ElabMultipleTopLevelModules), nullptr);
}

// ---- Instantiated modules ----

TEST_F(ReachabilityTest, TopLevelsAreReachable) {
  EXPECT_TRUE(isReachable(getModule("top")));
  EXPECT_TRUE(isReachable(getProgram("prog")));
}

TEST_F(ReachabilityTest, InstantiatedModuleIsReachable) {
  ASSERT_NE(getModule("leaf"), nullptr);
  EXPECT_TRUE(isReachable(getModule("leaf")));
}

TEST_F(ReachabilityTest, BoundModuleIsReachableThroughItsTarget) {
  ASSERT_NE(getModule("bound_mod"), nullptr);
  EXPECT_TRUE(isReachable(getModule("bound_mod")));
}

TEST_F(ReachabilityTest, NestedModuleWithoutPortsIsImplicitlyInstantiated) {
  ASSERT_NE(getNestedModule("nested_implicit"), nullptr);
  EXPECT_TRUE(isReachable(getNestedModule("nested_implicit")));
}

TEST_F(ReachabilityTest, NestedModuleWithPortsIsIgnored) {
  ASSERT_NE(getNestedModule("nested_ignored"), nullptr);
  EXPECT_FALSE(isReachable(getNestedModule("nested_ignored")));
}

TEST_F(ReachabilityTest, ModuleInstantiatedOnlyByAnIgnoredModuleIsNotReachable) {
  ASSERT_NE(getModule("orphan_leaf"), nullptr);
  EXPECT_FALSE(isReachable(getModule("orphan_leaf")));
}

TEST_F(ReachabilityTest, ModuleInstantiatedOnlyInAnUninstantiatedGenerateBlockIsNotReachable) {
  GTEST_SKIP() << "IEEE 1800-2023 Sec 27.3: the block of `if (0)` is not instantiated, so gen_only is not part "
                  "of the design. Generate constructs are not evaluated (no generate-branch selection), so "
                  "every branch's instantiations count as reachable.";
  ASSERT_NE(getModule("gen_only"), nullptr);
  EXPECT_FALSE(isReachable(getModule("gen_only")));
}

// ---- Classes and packages ----

TEST_F(ReachabilityTest, ClassUsedAsAVariableTypeIsReachable) {
  ASSERT_NE(getClass("Used"), nullptr);
  EXPECT_TRUE(isReachable(getClass("Used")));
}

TEST_F(ReachabilityTest, UnusedClassIsNotReachable) {
  ASSERT_NE(getClass("Unused"), nullptr);
  EXPECT_FALSE(isReachable(getClass("Unused")));
}

TEST_F(ReachabilityTest, ClassUsedThroughATypedefIsReachable) {
  ASSERT_NE(getClass("ViaTypedefBase"), nullptr);
  EXPECT_TRUE(isReachable(getClass("ViaTypedefBase")));
}

TEST_F(ReachabilityTest, ExtendedClassIsReachable) {
  ASSERT_NE(getClass("ExtDerived"), nullptr);
  ASSERT_NE(getClass("ExtBase"), nullptr);
  EXPECT_TRUE(isReachable(getClass("ExtDerived")));
  EXPECT_TRUE(isReachable(getClass("ExtBase")));
}

TEST_F(ReachabilityTest, SpecializationAndItsTypeArgumentAreReachable) {
  const hldb::ClassDefn *const holderTypeArg = getVariableClass("holder_h");
  ASSERT_NE(holderTypeArg, nullptr);
  ASSERT_NE(getClass("Holder"), nullptr);
  ASSERT_NE(holderTypeArg, getClass("Holder"));
  EXPECT_TRUE(isReachable(holderTypeArg));
  ASSERT_NE(getClass("TypeArg"), nullptr);
  EXPECT_TRUE(isReachable(getClass("TypeArg")));
}

TEST_F(ReachabilityTest, DefinitionUsedOnlyThroughItsSpecializationIsNotReachable) {
  ASSERT_NE(getClass("Holder"), nullptr);
  // Only Holder #(TypeArg) is used; Holder with its own default (int) is not.
  EXPECT_FALSE(isReachable(getClass("Holder")));
}

TEST_F(ReachabilityTest, ClassOfACalledStaticMethodIsReachable) {
  ASSERT_NE(getClass("Statics"), nullptr);
  EXPECT_TRUE(isReachable(getClass("Statics")));
}

TEST_F(ReachabilityTest, SelfReferencingClassIsReachable) {
  ASSERT_NE(getClass("SelfRef"), nullptr);
  EXPECT_TRUE(isReachable(getClass("SelfRef")));
}

TEST_F(ReachabilityTest, PackageOfACalledFunctionIsReachable) {
  ASSERT_NE(getPackage("util_pkg"), nullptr);
  EXPECT_TRUE(isReachable(getPackage("util_pkg")));
}

TEST_F(ReachabilityTest, UnusedClassInAReachablePackageIsNotReachable) {
  const hldb::Package *const pkg = getPackage("util_pkg");
  ASSERT_NE(pkg, nullptr);
  const hldb::ClassDefn *const cls = hldb::findByDefName<hldb::ClassDefn>("PkgUnused", pkg->getClassDefns());
  ASSERT_NE(cls, nullptr);
  EXPECT_FALSE(isReachable(cls));
}

TEST_F(ReachabilityTest, UnusedPackageIsNotReachable) {
  ASSERT_NE(getPackage("unused_pkg"), nullptr);
  EXPECT_FALSE(isReachable(getPackage("unused_pkg")));
}

TEST_F(ReachabilityTest, ItemOfAReachableModuleIsReachable) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const x = hldb::findByName<hldb::Variable>("x", top->getVariables());
  ASSERT_NE(x, nullptr);
  EXPECT_TRUE(isReachable(x));
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
