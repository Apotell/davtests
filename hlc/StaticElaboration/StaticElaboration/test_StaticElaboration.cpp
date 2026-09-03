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

// Tests for dut.sv -- see that file for the full source. This file documents the gap that
// "static elaboration" (definition-level parameter specialization -- see memory note
// project_static_elaboration_plan) is meant to close, across all four parameterizable
// definition kinds (module, interface, program, class), plus the harder scoped-lookup cases:
// a class nested in a package, a class nested in another class, the SAME nested-class name
// reused under two different outer scopes, and a parameterized module instantiated from a
// non-top-level module.
//
// IEEE 1800-2023 Sec 6.20 (Parameter and constant declarations) / Sec 23.3 (Module
// instantiation) require that a differently-parameterized use be bound against a distinct,
// independently specialized definition -- a reference to a parameter anywhere in the
// definition's own body (not just its own parameter port list) must resolve against the value
// supplied at THAT use, never the definition's own default.
//
// A per-instance/per-handle #(...) override lives on that use site's own <Kind>Typespec
// (module_typespec.yaml/interface_typespec.yaml/program_typespec.yaml/class_typespec.yaml's
// own param_assign field) -- NEVER on a RefInstance, which extends Any directly (not Scope),
// so it can never appear in a Typespec's own parent chain; this is also the only design that
// survives a single #(...) shared across a comma-separated hierarchical_instance list (IEEE
// 1800-2023 Annex A.4.1.1), since each instance already gets its own distinct typespec to hold
// its own copy. RefInstance is only ever used below as a private lookup step (find the named
// instance, then its typespec) -- never as the place override data is asserted against.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/RTTI.h>
#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/interface_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/program.h>
#include <hldb/program_typespec.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>

namespace hlc {

class StaticElaborationTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "StaticElaboration.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // ---- top-level definition lookups ----

  static const hldb::Module *getDut() { return hldb::findByName<hldb::Module>("dut", m_design->getAllModules()); }
  static const hldb::Module *getMidLevel() {
    return hldb::findByName<hldb::Module>("mid_level", m_design->getAllModules());
  }

  // The template/base definitions -- never mutated by static elaboration; a specialization is
  // always a separate, additional object.
  static const hldb::Module *getParamMod() {
    return hldb::findByName<hldb::Module>("param_mod", m_design->getAllModules());
  }
  static const hldb::Interface *getParamIf() {
    return hldb::findByName<hldb::Interface>("param_if", m_design->getAllInterfaces());
  }
  static const hldb::Program *getParamProg() {
    return hldb::findByName<hldb::Program>("param_prog", m_design->getAllPrograms());
  }
  static const hldb::ClassDefn *getParamCls() {
    return hldb::findByName<hldb::ClassDefn>("param_cls", m_design->getAllClasses());
  }
  static const hldb::Package *getPkgA() { return hldb::findByName<hldb::Package>("pkg_a", m_design->getAllPackages()); }
  static const hldb::Package *getPkgB() { return hldb::findByName<hldb::Package>("pkg_b", m_design->getAllPackages()); }
  static const hldb::ClassDefn *getOuterCls() {
    return hldb::findByName<hldb::ClassDefn>("OuterCls", m_design->getAllClasses());
  }
  static const hldb::ClassDefn *getOtherOuterCls() {
    return hldb::findByName<hldb::ClassDefn>("OtherOuterCls", m_design->getAllClasses());
  }
  static const hldb::ClassDefn *getNestedClass(const hldb::ClassDefn *outer, std::string_view name) {
    if ((outer == nullptr) || (outer->getClassDefns() == nullptr)) return nullptr;
    return hldb::findByName<hldb::ClassDefn>(name, outer->getClassDefns());
  }
  static const hldb::ClassDefn *getPkgClass(const hldb::Package *pkg, std::string_view name) {
    if ((pkg == nullptr) || (pkg->getClassDefns() == nullptr)) return nullptr;
    return hldb::findByName<hldb::ClassDefn>(name, pkg->getClassDefns());
  }
  static const hldb::Package *getPkgDeep() {
    return hldb::findByName<hldb::Package>("pkg_deep", m_design->getAllPackages());
  }

  // Walks a scoped ClassTypespec's own path_elems[index] and returns that segment's own
  // resolved ClassTypespec (via RefTypespec::actual) -- used to inspect an INTERMEDIATE
  // segment's own specialization in a multi-level chain (e.g. pkg_deep::Level1::Level2::Level3),
  // where every segment gets its own ClassTypespec distinct from the chain's final result (the
  // last segment's own ClassTypespec self-references the chain's own return value -- see
  // Phase3ModelBuilder::resolveScopedUnsupportedTypespec).
  static const hldb::ClassTypespec *getPathElemClassTypespec(const hldb::ClassTypespec *ct, size_t index) {
    if ((ct == nullptr) || (ct->getPathElems() == nullptr) || (index >= ct->getPathElems()->size())) return nullptr;
    const hldb::RefTypespec *const segRef = any_cast<hldb::RefTypespec>(ct->getPathElems()->at(index));
    if (segRef == nullptr) return nullptr;
    return segRef->getActual<hldb::ClassTypespec>();
  }

  // ---- instance/handle typespec lookups ----
  //
  // RefInstance is used here purely as a private lookup step (find the named instance, then
  // the <Kind>Typespec its own RefTypespec::actual points at) -- never exposed to callers, and
  // never where override data is checked (see the file-level comment above for why).

  template <typename TypespecT>
  static const TypespecT *getInstanceTypespec(std::string_view instanceName, const hldb::Module *scope) {
    if ((scope == nullptr) || (scope->getRefInstances() == nullptr)) return nullptr;
    const hldb::RefInstance *const inst = hldb::findByName<hldb::RefInstance>(instanceName, scope->getRefInstances());
    if ((inst == nullptr) || (inst->getTypespec() == nullptr)) return nullptr;
    return inst->getTypespec()->getActual<TypespecT>();
  }

  static const hldb::ModuleTypespec *getInstDefaultTypespec() {
    return getInstanceTypespec<hldb::ModuleTypespec>("inst_default", getDut());
  }
  static const hldb::ModuleTypespec *getInstWideTypespec() {
    return getInstanceTypespec<hldb::ModuleTypespec>("inst_wide", getDut());
  }
  static const hldb::ModuleTypespec *getMidInstTypespec() {
    return getInstanceTypespec<hldb::ModuleTypespec>("mid_inst", getDut());
  }
  static const hldb::ModuleTypespec *getMidNestedSameAsWideTypespec() {
    return getInstanceTypespec<hldb::ModuleTypespec>("inst_nested_same_as_wide", getMidLevel());
  }
  static const hldb::ModuleTypespec *getMidNestedDistinctTypespec() {
    return getInstanceTypespec<hldb::ModuleTypespec>("inst_nested_distinct", getMidLevel());
  }

  static const hldb::InterfaceTypespec *getIfDefaultTypespec() {
    return getInstanceTypespec<hldb::InterfaceTypespec>("if_default", getDut());
  }
  static const hldb::InterfaceTypespec *getIfWideTypespec() {
    return getInstanceTypespec<hldb::InterfaceTypespec>("if_wide", getDut());
  }

  static const hldb::ProgramTypespec *getProgDefaultTypespec() {
    return getInstanceTypespec<hldb::ProgramTypespec>("prog_default", getDut());
  }
  static const hldb::ProgramTypespec *getProgWideTypespec() {
    return getInstanceTypespec<hldb::ProgramTypespec>("prog_wide", getDut());
  }

  // A class handle is a plain Variable, not a RefInstance -- its ClassTypespec is reached via
  // Variable::getTypespec<RefTypespec>()->getActual<ClassTypespec>(), matching the existing,
  // already-passing Google/chapter-8/8.5--parameters davtests convention.
  static const hldb::ClassTypespec *getHandleTypespec(std::string_view handleName) {
    const hldb::Module *const dut = getDut();
    if ((dut == nullptr) || (dut->getVariables() == nullptr)) return nullptr;
    const hldb::Variable *const handle = hldb::findByName<hldb::Variable>(handleName, dut->getVariables());
    if ((handle == nullptr) || (handle->getTypespec() == nullptr)) return nullptr;
    const hldb::RefTypespec *const rt = handle->getTypespec<hldb::RefTypespec>();
    if (rt == nullptr) return nullptr;
    return rt->getActual<hldb::ClassTypespec>();
  }

  // ---- generic per-definition helpers (Module/Interface/Program/ClassDefn all share this
  // shape, inherited from scope.yaml) ----

  template <typename T>
  static const hldb::ParamAssign *getParamAssignByName(const T *scope, std::string_view name) {
    if ((scope == nullptr) || (scope->getParamAssigns() == nullptr)) return nullptr;
    for (const hldb::ParamAssign *const pa : *scope->getParamAssigns()) {
      if ((pa != nullptr) && (pa->getLhs() != nullptr) && (pa->getLhs()->getName() == name)) return pa;
    }
    return nullptr;
  }

  template <typename T>
  static const hldb::Variable *getVariableByName(const T *scope, std::string_view name) {
    if ((scope == nullptr) || (scope->getVariables() == nullptr)) return nullptr;
    return hldb::findByName<hldb::Variable>(name, scope->getVariables());
  }
};

// =============================================================================================
// Module (baseline, from before Task 7 -- kept for the "no regression" net)
// =============================================================================================

TEST_F(StaticElaborationTest, DutModuleExists) { EXPECT_NE(getDut(), nullptr); }

TEST_F(StaticElaborationTest, ParamModModuleExists) { EXPECT_NE(getParamMod(), nullptr); }

TEST_F(StaticElaborationTest, ParamModHasOneParameterW) {
  const hldb::Module *const paramMod = getParamMod();
  ASSERT_NE(paramMod, nullptr);
  ASSERT_NE(paramMod->getParameters(), nullptr);
  ASSERT_EQ(paramMod->getParameters()->size(), 1u);
  const hldb::Parameter *const w = any_cast<hldb::Parameter>(paramMod->getParameters()->at(0));
  ASSERT_NE(w, nullptr);
  EXPECT_EQ(w->getName(), std::string_view("W"));
  EXPECT_FALSE(w->getLocalParam()) << "'parameter int W = 8' is a parameter, not a localparam";
}

TEST_F(StaticElaborationTest, ParamModDefaultAssignForWIsEight) {
  const hldb::ParamAssign *const pa = getParamAssignByName(getParamMod(), "W");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), std::string_view("8"));
  EXPECT_FALSE(pa->getOverridden()) << "param_mod's own default for W is not an override";
}

TEST_F(StaticElaborationTest, BothInstanceTypespecsExist) {
  EXPECT_NE(getInstDefaultTypespec(), nullptr);
  EXPECT_NE(getInstWideTypespec(), nullptr);
}

TEST_F(StaticElaborationTest, InstDefaultHasNoOverrides) {
  const hldb::ModuleTypespec *const ts = getInstDefaultTypespec();
  ASSERT_NE(ts, nullptr);
  EXPECT_TRUE((ts->getParamAssigns() == nullptr) || ts->getParamAssigns()->empty());
}

TEST_F(StaticElaborationTest, InstWideOverridesWToSixteen) {
  const hldb::ModuleTypespec *const ts = getInstWideTypespec();
  ASSERT_NE(ts, nullptr);
  ASSERT_NE(ts->getParamAssigns(), nullptr);
  ASSERT_EQ(ts->getParamAssigns()->size(), 1u);
  const hldb::ParamAssign *const pa = ts->getParamAssigns()->at(0);
  ASSERT_NE(pa, nullptr);
  ASSERT_NE(pa->getLhs(), nullptr);
  EXPECT_EQ(pa->getLhs()->getName(), std::string_view("W"));
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), std::string_view("16"));
  EXPECT_TRUE(pa->getOverridden());
  EXPECT_TRUE(pa->getConnByName()) << "'.W(16)' is a by-name connection";
}

TEST_F(StaticElaborationTest, InstDefaultResolvesToBaseDefinition) {
  const hldb::ModuleTypespec *const ts = getInstDefaultTypespec();
  ASSERT_NE(ts, nullptr);
  EXPECT_EQ(ts->getModule(), getParamMod());
}

TEST_F(StaticElaborationTest, InstWideResolvesToDistinctSpecializedDefinition) {
  const hldb::Module *const modDefault = getInstDefaultTypespec()->getModule();
  const hldb::Module *const modWide = getInstWideTypespec()->getModule();
  ASSERT_NE(modDefault, nullptr);
  ASSERT_NE(modWide, nullptr) << "inst_wide's override must resolve to a specialized Module";
  EXPECT_NE(modWide, modDefault);
  EXPECT_NE(modWide, getParamMod());
}

TEST_F(StaticElaborationTest, SpecializedModuleHasWOverriddenToSixteen) {
  const hldb::Module *const modWide = getInstWideTypespec()->getModule();
  ASSERT_NE(modWide, nullptr);
  const hldb::ParamAssign *const pa = getParamAssignByName(modWide, "W");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), std::string_view("16"));
}

// Confirms the full-clone build strategy (as opposed to a delta/overlay one that shares
// parameter-independent children by pointer): a plain variable declaration elsewhere in the
// module body must be its OWN, independent object on the specialization, not shared with the
// template. See project_static_elaboration_plan's own "Building a specialization: full clone"
// section for why delta/overlay was rejected.
TEST_F(StaticElaborationTest, SpecializedModuleOwnsItsOwnCopyOfInternalReg) {
  const hldb::Module *const modWide = getInstWideTypespec()->getModule();
  ASSERT_NE(modWide, nullptr);
  const hldb::Variable *const baseReg = getVariableByName(getParamMod(), "internal_reg");
  const hldb::Variable *const specializedReg = getVariableByName(modWide, "internal_reg");
  ASSERT_NE(baseReg, nullptr);
  ASSERT_NE(specializedReg, nullptr);
  EXPECT_NE(specializedReg, baseReg);
}

TEST_F(StaticElaborationTest, CompilerReportsNoErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
}

// =============================================================================================
// Interface (Task 7)
// =============================================================================================

TEST_F(StaticElaborationTest, ParamIfInterfaceExists) { EXPECT_NE(getParamIf(), nullptr); }

TEST_F(StaticElaborationTest, ParamIfHasOneParameterW) {
  const hldb::Interface *const paramIf = getParamIf();
  ASSERT_NE(paramIf, nullptr);
  ASSERT_NE(paramIf->getParameters(), nullptr);
  ASSERT_EQ(paramIf->getParameters()->size(), 1u);
  const hldb::Parameter *const w = any_cast<hldb::Parameter>(paramIf->getParameters()->at(0));
  ASSERT_NE(w, nullptr);
  EXPECT_EQ(w->getName(), std::string_view("W"));
}

TEST_F(StaticElaborationTest, IfDefaultResolvesToBaseInterface) {
  const hldb::InterfaceTypespec *const ts = getIfDefaultTypespec();
  ASSERT_NE(ts, nullptr);
  EXPECT_EQ(ts->getInterface(), getParamIf());
}

TEST_F(StaticElaborationTest, IfWideResolvesToDistinctSpecializedInterface) {
  const hldb::InterfaceTypespec *const tsDefault = getIfDefaultTypespec();
  const hldb::InterfaceTypespec *const tsWide = getIfWideTypespec();
  ASSERT_NE(tsDefault, nullptr);
  ASSERT_NE(tsWide, nullptr);
  const hldb::Interface *const ifDefault = tsDefault->getInterface();
  const hldb::Interface *const ifWide = tsWide->getInterface();
  ASSERT_NE(ifDefault, nullptr);
  ASSERT_NE(ifWide, nullptr);
  EXPECT_NE(ifWide, ifDefault);
  EXPECT_NE(ifWide, getParamIf());
}

TEST_F(StaticElaborationTest, SpecializedInterfaceHasWOverriddenToEight) {
  const hldb::Interface *const ifWide = getIfWideTypespec()->getInterface();
  ASSERT_NE(ifWide, nullptr);
  const hldb::ParamAssign *const pa = getParamAssignByName(ifWide, "W");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), std::string_view("8"));
}

TEST_F(StaticElaborationTest, SpecializedInterfaceOwnsItsOwnCopyOfData) {
  const hldb::Interface *const ifWide = getIfWideTypespec()->getInterface();
  ASSERT_NE(ifWide, nullptr);
  const hldb::Variable *const baseData = getVariableByName(getParamIf(), "data");
  const hldb::Variable *const specializedData = getVariableByName(ifWide, "data");
  ASSERT_NE(baseData, nullptr);
  ASSERT_NE(specializedData, nullptr);
  EXPECT_NE(specializedData, baseData);
}

// =============================================================================================
// Program (Task 7)
// =============================================================================================

TEST_F(StaticElaborationTest, ParamProgProgramExists) { EXPECT_NE(getParamProg(), nullptr); }

TEST_F(StaticElaborationTest, ProgDefaultResolvesToBaseProgram) {
  const hldb::ProgramTypespec *const ts = getProgDefaultTypespec();
  ASSERT_NE(ts, nullptr);
  EXPECT_EQ(ts->getProgram(), getParamProg());
}

TEST_F(StaticElaborationTest, ProgWideResolvesToDistinctSpecializedProgram) {
  const hldb::ProgramTypespec *const tsDefault = getProgDefaultTypespec();
  const hldb::ProgramTypespec *const tsWide = getProgWideTypespec();
  ASSERT_NE(tsDefault, nullptr);
  ASSERT_NE(tsWide, nullptr);
  const hldb::Program *const progDefault = tsDefault->getProgram();
  const hldb::Program *const progWide = tsWide->getProgram();
  ASSERT_NE(progDefault, nullptr);
  ASSERT_NE(progWide, nullptr);
  EXPECT_NE(progWide, progDefault);
  EXPECT_NE(progWide, getParamProg());
}

TEST_F(StaticElaborationTest, SpecializedProgramHasWOverriddenToFour) {
  const hldb::Program *const progWide = getProgWideTypespec()->getProgram();
  ASSERT_NE(progWide, nullptr);
  const hldb::ParamAssign *const pa = getParamAssignByName(progWide, "W");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), std::string_view("4"));
}

// =============================================================================================
// Class -- plain, no package/nesting (Task 7)
// =============================================================================================

TEST_F(StaticElaborationTest, ParamClsClassExists) { EXPECT_NE(getParamCls(), nullptr); }

TEST_F(StaticElaborationTest, ClsDefaultHandleResolvesToBaseClass) {
  const hldb::ClassTypespec *const ct = getHandleTypespec("cls_default_handle");
  ASSERT_NE(ct, nullptr);
  EXPECT_EQ(ct->getClassDefn(), getParamCls());
}

TEST_F(StaticElaborationTest, ClsWideHandleResolvesToDistinctSpecializedClass) {
  const hldb::ClassTypespec *const ctDefault = getHandleTypespec("cls_default_handle");
  const hldb::ClassTypespec *const ctWide = getHandleTypespec("cls_wide_handle");
  ASSERT_NE(ctDefault, nullptr);
  ASSERT_NE(ctWide, nullptr);
  const hldb::ClassDefn *const clsDefault = ctDefault->getClassDefn();
  const hldb::ClassDefn *const clsWide = ctWide->getClassDefn();
  ASSERT_NE(clsDefault, nullptr);
  ASSERT_NE(clsWide, nullptr);
  EXPECT_NE(clsWide, clsDefault);
  EXPECT_NE(clsWide, getParamCls());
}

TEST_F(StaticElaborationTest, SpecializedClsHasWOverriddenToNine) {
  const hldb::ClassDefn *const clsWide = getHandleTypespec("cls_wide_handle")->getClassDefn();
  ASSERT_NE(clsWide, nullptr);
  const hldb::ParamAssign *const pa = getParamAssignByName(clsWide, "W");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), std::string_view("9"));
}

TEST_F(StaticElaborationTest, SpecializedClsOwnsItsOwnCopyOfPayload) {
  const hldb::ClassDefn *const clsWide = getHandleTypespec("cls_wide_handle")->getClassDefn();
  ASSERT_NE(clsWide, nullptr);
  const hldb::Variable *const basePayload = getVariableByName(getParamCls(), "payload");
  const hldb::Variable *const specializedPayload = getVariableByName(clsWide, "payload");
  ASSERT_NE(basePayload, nullptr);
  ASSERT_NE(specializedPayload, nullptr);
  EXPECT_NE(specializedPayload, basePayload);
}

// =============================================================================================
// Class nested inside a package (Task 5 + Task 7) -- pkg_a::PkgWidget / pkg_b::PkgWidget, same
// nested class name under two different packages.
// =============================================================================================

TEST_F(StaticElaborationTest, PkgAAndPkgBExist) {
  EXPECT_NE(getPkgA(), nullptr);
  EXPECT_NE(getPkgB(), nullptr);
}

TEST_F(StaticElaborationTest, PkgAAndPkgBEachHaveTheirOwnDistinctPkgWidget) {
  const hldb::ClassDefn *const widgetA = getPkgClass(getPkgA(), "PkgWidget");
  const hldb::ClassDefn *const widgetB = getPkgClass(getPkgB(), "PkgWidget");
  ASSERT_NE(widgetA, nullptr);
  ASSERT_NE(widgetB, nullptr);
  EXPECT_NE(widgetA, widgetB) << "pkg_a::PkgWidget and pkg_b::PkgWidget share a name but must be "
                                 "distinct ClassDefn objects";
}

TEST_F(StaticElaborationTest, PkgAHandleResolvesToSpecializationOfPkgAsWidget) {
  const hldb::ClassTypespec *const ct = getHandleTypespec("pkg_a_handle");
  ASSERT_NE(ct, nullptr);
  const hldb::ClassDefn *const specialized = ct->getClassDefn();
  ASSERT_NE(specialized, nullptr) << "pkg_a::PkgWidget#(10) must resolve -- scoped lookup must "
                                     "search pkg_a's own class list, not a flat/global one";
  EXPECT_NE(specialized, getPkgClass(getPkgA(), "PkgWidget"))
      << "an override-bearing use must specialize, not bind straight to the template";
  const hldb::ParamAssign *const pa = getParamAssignByName(specialized, "W");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), std::string_view("10"));
}

TEST_F(StaticElaborationTest, PkgBHandleResolvesToSpecializationOfPkgBsWidget) {
  const hldb::ClassTypespec *const ct = getHandleTypespec("pkg_b_handle");
  ASSERT_NE(ct, nullptr);
  const hldb::ClassDefn *const specialized = ct->getClassDefn();
  ASSERT_NE(specialized, nullptr) << "pkg_b::PkgWidget#(20) must resolve against pkg_b's OWN "
                                     "PkgWidget, not be confused with pkg_a's same-named one";
  EXPECT_NE(specialized, getPkgClass(getPkgB(), "PkgWidget"));
  EXPECT_NE(specialized, getHandleTypespec("pkg_a_handle")->getClassDefn())
      << "pkg_a's and pkg_b's specializations must never collide, even though both PkgWidget "
         "classes share a name";
  const hldb::ParamAssign *const pa = getParamAssignByName(specialized, "W");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), std::string_view("20"));
}

// =============================================================================================
// Class nested inside another class (Task 5 + Task 7) -- OuterCls::InnerCls /
// OtherOuterCls::InnerCls, same nested class name under two different outer classes.
// =============================================================================================

TEST_F(StaticElaborationTest, OuterClsAndOtherOuterClsExist) {
  EXPECT_NE(getOuterCls(), nullptr);
  EXPECT_NE(getOtherOuterCls(), nullptr);
}

TEST_F(StaticElaborationTest, OuterClsAndOtherOuterClsEachHaveTheirOwnDistinctInnerCls) {
  const hldb::ClassDefn *const innerA = getNestedClass(getOuterCls(), "InnerCls");
  const hldb::ClassDefn *const innerB = getNestedClass(getOtherOuterCls(), "InnerCls");
  ASSERT_NE(innerA, nullptr);
  ASSERT_NE(innerB, nullptr);
  EXPECT_NE(innerA, innerB) << "OuterCls::InnerCls and OtherOuterCls::InnerCls share a name but "
                               "must be distinct ClassDefn objects";
}

TEST_F(StaticElaborationTest, OuterInnerHandleResolvesToSpecializationOfOuterClsInnerCls) {
  const hldb::ClassTypespec *const ct = getHandleTypespec("outer_inner_handle");
  ASSERT_NE(ct, nullptr);
  const hldb::ClassDefn *const specialized = ct->getClassDefn();
  ASSERT_NE(specialized, nullptr) << "OuterCls::InnerCls#(12) must resolve -- scoped lookup must "
                                     "search OuterCls's own nested class list";
  EXPECT_NE(specialized, getNestedClass(getOuterCls(), "InnerCls"));
  const hldb::ParamAssign *const pa = getParamAssignByName(specialized, "W");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), std::string_view("12"));
}

TEST_F(StaticElaborationTest, OtherOuterInnerHandleResolvesToSpecializationOfOtherOuterClsInnerCls) {
  const hldb::ClassTypespec *const ct = getHandleTypespec("other_outer_inner_handle");
  ASSERT_NE(ct, nullptr);
  const hldb::ClassDefn *const specialized = ct->getClassDefn();
  ASSERT_NE(specialized, nullptr) << "OtherOuterCls::InnerCls#(24) must resolve against "
                                     "OtherOuterCls's OWN InnerCls, not be confused with "
                                     "OuterCls's same-named one";
  EXPECT_NE(specialized, getNestedClass(getOtherOuterCls(), "InnerCls"));
  EXPECT_NE(specialized, getHandleTypespec("outer_inner_handle")->getClassDefn())
      << "OuterCls's and OtherOuterCls's specializations must never collide, even though both "
         "InnerCls classes share a name";
  const hldb::ParamAssign *const pa = getParamAssignByName(specialized, "W");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), std::string_view("24"));
}

// =============================================================================================
// Deeply nested class chain (Task 7 depth generalization) -- pkg_deep::Level1::Level2::Level3,
// a 4-segment chain with an independent override at every level. IEEE 1800-2023 places no cap
// on scope-resolution nesting depth (PkgA::ClassA::ClassB::ClassC is exactly as legal as
// PkgA::ClassA); confirms Phase3ModelBuilder::resolveScopedUnsupportedTypespec() walks and
// specializes every segment of an arbitrarily long chain, not just a single Scope::Target pair.
// =============================================================================================

TEST_F(StaticElaborationTest, PkgDeepAndNestedChainClassesExist) {
  const hldb::Package *const pkgDeep = getPkgDeep();
  ASSERT_NE(pkgDeep, nullptr);
  const hldb::ClassDefn *const level1 = getPkgClass(pkgDeep, "Level1");
  ASSERT_NE(level1, nullptr);
  const hldb::ClassDefn *const level2 = getNestedClass(level1, "Level2");
  ASSERT_NE(level2, nullptr);
  const hldb::ClassDefn *const level3 = getNestedClass(level2, "Level3");
  EXPECT_NE(level3, nullptr);
}

TEST_F(StaticElaborationTest, DeepHandleResolvesToSpecializedLevel3) {
  const hldb::ClassTypespec *const ct = getHandleTypespec("deep_handle");
  ASSERT_NE(ct, nullptr);
  const hldb::ClassDefn *const specialized = ct->getClassDefn();
  ASSERT_NE(specialized, nullptr) << "pkg_deep::Level1#(11)::Level2#(22)::Level3#(33) must fully "
                                      "resolve -- a 4-segment chain, not just a 2-segment one";
  const hldb::ClassDefn *const baseLevel1 = getPkgClass(getPkgDeep(), "Level1");
  const hldb::ClassDefn *const baseLevel2 = getNestedClass(baseLevel1, "Level2");
  const hldb::ClassDefn *const baseLevel3 = getNestedClass(baseLevel2, "Level3");
  ASSERT_NE(baseLevel3, nullptr);
  EXPECT_NE(specialized, baseLevel3) << "an override-bearing use must specialize, not bind "
                                         "straight to the template";
  const hldb::ParamAssign *const pa = getParamAssignByName(specialized, "W");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), std::string_view("33"));
}

TEST_F(StaticElaborationTest, DeepHandleLevel1SegmentIsSpecialized) {
  const hldb::ClassTypespec *const ct = getHandleTypespec("deep_handle");
  ASSERT_NE(ct, nullptr);
  // path_elems: [0] = pkg_deep scope, [1] = Level1, [2] = Level2, [3] = Level3 (== ct itself).
  const hldb::ClassTypespec *const level1Ct = getPathElemClassTypespec(ct, 1);
  ASSERT_NE(level1Ct, nullptr);
  const hldb::ClassDefn *const specializedLevel1 = level1Ct->getClassDefn();
  ASSERT_NE(specializedLevel1, nullptr);
  EXPECT_NE(specializedLevel1, getPkgClass(getPkgDeep(), "Level1"))
      << "an intermediate segment's own #(...) must specialize it too, not just the leaf";
  const hldb::ParamAssign *const pa = getParamAssignByName(specializedLevel1, "W");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), std::string_view("11"));
}

TEST_F(StaticElaborationTest, DeepHandleLevel2SegmentIsSpecializedWithinLevel1sSpecialization) {
  const hldb::ClassTypespec *const ct = getHandleTypespec("deep_handle");
  ASSERT_NE(ct, nullptr);
  const hldb::ClassTypespec *const level1Ct = getPathElemClassTypespec(ct, 1);
  ASSERT_NE(level1Ct, nullptr);
  const hldb::ClassTypespec *const level2Ct = getPathElemClassTypespec(ct, 2);
  ASSERT_NE(level2Ct, nullptr);
  const hldb::ClassDefn *const specializedLevel2 = level2Ct->getClassDefn();
  ASSERT_NE(specializedLevel2, nullptr);
  EXPECT_NE(specializedLevel2, getNestedClass(getPkgClass(getPkgDeep(), "Level1"), "Level2"))
      << "must not be Level1's BASE own Level2 -- Level2 must be looked up within Level1's own "
         "SPECIALIZATION, since a nested class's own fields could depend on the enclosing "
         "specialization's parameter";
  // getNestedClass(level1Ct->getClassDefn(), "Level2") is Level2 as found WITHIN Level1's own
  // specialization -- i.e. what Level2's own #(22) override still needed to be applied to.
  // specializedLevel2 is that same lookup's result AFTER its own override was applied, so the
  // two must be distinct objects too -- confirming Level2's own #(...) specialized it a second,
  // independent time on top of being resolved in the right (specialized) scope.
  EXPECT_NE(specializedLevel2, getNestedClass(level1Ct->getClassDefn(), "Level2"));
  const hldb::ParamAssign *const pa = getParamAssignByName(specializedLevel2, "W");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), std::string_view("22"));
}

TEST_F(StaticElaborationTest, DeepHandleLastSegmentSelfReferencesTheChainsOwnResult) {
  const hldb::ClassTypespec *const ct = getHandleTypespec("deep_handle");
  ASSERT_NE(ct, nullptr);
  ASSERT_NE(ct->getPathElems(), nullptr);
  ASSERT_EQ(ct->getPathElems()->size(), 4u);
  EXPECT_EQ(getPathElemClassTypespec(ct, 3), ct)
      << "for the LAST segment of a scoped chain, its own resolution IS the whole reference's "
         "resolution -- path_elems[N-1]'s own actual must be the chain's own final result, not "
         "a separate object";
}

// =============================================================================================
// Module within module -- a parameterized module instantiated from a non-top-level module.
// Confirms specialization (and its cross-scope dedup) is independent of hierarchy depth.
// =============================================================================================

TEST_F(StaticElaborationTest, MidLevelModuleExists) { EXPECT_NE(getMidLevel(), nullptr); }

TEST_F(StaticElaborationTest, MidInstResolvesToBaseMidLevel) {
  // 'mid_level mid_inst ();' supplies no #(...) -- mid_level itself is never specialized.
  const hldb::ModuleTypespec *const ts = getMidInstTypespec();
  ASSERT_NE(ts, nullptr);
  EXPECT_EQ(ts->getModule(), getMidLevel());
}

TEST_F(StaticElaborationTest, MidNestedSameAsWideDedupesWithDutInstWide) {
  // mid_level's own inst_nested_same_as_wide uses the SAME override value (W=16) as dut's
  // inst_wide -- despite living in a completely different enclosing scope, both must resolve
  // to the exact same specialization object.
  const hldb::ModuleTypespec *const tsFromDut = getInstWideTypespec();
  const hldb::ModuleTypespec *const tsFromMid = getMidNestedSameAsWideTypespec();
  ASSERT_NE(tsFromDut, nullptr);
  ASSERT_NE(tsFromMid, nullptr);
  const hldb::Module *const modFromDut = tsFromDut->getModule();
  const hldb::Module *const modFromMid = tsFromMid->getModule();
  ASSERT_NE(modFromDut, nullptr);
  ASSERT_NE(modFromMid, nullptr);
  EXPECT_EQ(modFromMid, modFromDut) << "the same override value (W=16), requested from two "
                                        "different enclosing scopes, must dedupe to one "
                                        "specialization";
}

TEST_F(StaticElaborationTest, MidNestedDistinctIsItsOwnSpecialization) {
  const hldb::ModuleTypespec *const tsDistinct = getMidNestedDistinctTypespec();
  const hldb::ModuleTypespec *const tsSameAsWide = getMidNestedSameAsWideTypespec();
  ASSERT_NE(tsDistinct, nullptr);
  ASSERT_NE(tsSameAsWide, nullptr);
  const hldb::Module *const modDistinct = tsDistinct->getModule();
  const hldb::Module *const modSameAsWide = tsSameAsWide->getModule();
  ASSERT_NE(modDistinct, nullptr);
  ASSERT_NE(modSameAsWide, nullptr);
  EXPECT_NE(modDistinct, modSameAsWide);
  EXPECT_NE(modDistinct, getParamMod());

  const hldb::ParamAssign *const pa = getParamAssignByName(modDistinct, "W");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), std::string_view("24"));
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
