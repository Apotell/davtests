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

#include <hldb/Database.h>
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
#include <hldb/typedef_typespec.h>
#include <hldb/unsupported_typespec.h>
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
  static const hldb::Package *getPkgTd() { return hldb::findByName<hldb::Package>("pkg_td", m_design->getAllPackages()); }

  // TdOuter is declared directly inside `dut` (a Module), not at file/package scope like
  // param_cls/OuterCls/etc. -- m_design->getAllClasses() does NOT include it (confirmed: that
  // Design-level aggregate only reflects file-scope class declarations; Phase3ModelBuilder's
  // own internal m_classes map is unaffected, since it is built from
  // Database::getObjects<ClassDefn>(), a flat, unconditional sweep of every ClassDefn object
  // ever created, not from this curated aggregate -- which is why production resolution of
  // TdOuter through the typedef path still works correctly despite this). Search `owner`'s own
  // getInstanceItems() instead -- the polymorphic collection a module-local class declaration
  // actually lands in (scope.yaml's own `instance_item` field, which module.yaml inherits).
  static const hldb::ClassDefn *getModuleLocalClass(const hldb::Module *owner, std::string_view name) {
    if ((owner == nullptr) || (owner->getClassDefns() == nullptr)) return nullptr;
    return hldb::findByName<hldb::ClassDefn>(name, owner->getClassDefns());
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

  // A typedef'd handle's own RefTypespec::actual is the TypedefTypespec itself -- Phase3
  // deliberately never dereferences through a typedef automatically (see
  // Phase3ModelBuilder::findTypedefByName()'s own comment: consumers are expected to follow
  // getTypedefAlias() themselves) -- so getHandleTypespec() (which expects a ClassTypespec
  // directly) does not apply to a typedef'd handle. This is the typedef analog of it.
  static const hldb::TypedefTypespec *getHandleTypedefTypespec(std::string_view handleName) {
    const hldb::Module *const dut = getDut();
    if ((dut == nullptr) || (dut->getVariables() == nullptr)) return nullptr;
    const hldb::Variable *const handle = hldb::findByName<hldb::Variable>(handleName, dut->getVariables());
    if ((handle == nullptr) || (handle->getTypespec() == nullptr)) return nullptr;
    const hldb::RefTypespec *const rt = handle->getTypespec<hldb::RefTypespec>();
    if (rt == nullptr) return nullptr;
    return rt->getActual<hldb::TypedefTypespec>();
  }

  // Follows a TypedefTypespec's own typedef_alias to whatever ClassTypespec it ultimately
  // resolved to (the aliased type's own use-site typespec, specialized if it carried an
  // override) -- nullptr if the alias itself resolved to something else (e.g. another typedef;
  // see getHandleTypedefTypespec() applied a second time for that case).
  static const hldb::ClassTypespec *getTypedefAliasClassTypespec(const hldb::TypedefTypespec *td) {
    if ((td == nullptr) || (td->getTypedefAlias() == nullptr)) return nullptr;
    return td->getTypedefAlias()->getActual<hldb::ClassTypespec>();
  }

  // Same idea as getHandleTypedefTypespec(), but for a plain data member of some OTHER class
  // (not dut itself) -- used to confirm a bare typedef reference resolves via
  // Phase3ModelBuilder::findTypedefInEnclosingScopes()'s walk up the enclosing-scope chain,
  // not just the member's own immediate scope.
  static const hldb::TypedefTypespec *getMemberTypedefTypespec(const hldb::ClassDefn *owner,
                                                                 std::string_view memberName) {
    if ((owner == nullptr) || (owner->getVariables() == nullptr)) return nullptr;
    const hldb::Variable *const member = hldb::findByName<hldb::Variable>(memberName, owner->getVariables());
    if ((member == nullptr) || (member->getTypespec() == nullptr)) return nullptr;
    const hldb::RefTypespec *const rt = member->getTypespec<hldb::RefTypespec>();
    if (rt == nullptr) return nullptr;
    return rt->getActual<hldb::TypedefTypespec>();
  }

  // ---- whole-graph counting helpers ----
  //
  // Database::getObjects<T>() is a flat, unconditional sweep of every object of that exact
  // type ever created anywhere in the WHOLE compilation -- unlike m_design->getAllClasses()
  // (confirmed elsewhere in this file NOT to include module-nested classes), this is the same
  // registry Phase3ModelBuilder itself reads from, so a count taken this way is a genuine,
  // ground-truth object-graph census, not a curated aggregate that could itself be incomplete.
  // Used below to assert exact object counts across the whole StaticElaboration compilation --
  // a coarser, whole-graph counterpart to the individual dedup/identity checks above: those
  // confirm a handful of SPECIFIC use sites resolve correctly, but a stray extra (or missing)
  // object anywhere else in the file -- e.g. exactly the orphaned, wrongly-based duplicate
  // ClassDefns Task 10's fifth follow-up found -- would slip past every one of them, since none
  // of them ever inspects the graph as a whole. These counts would have caught that bug
  // directly (one extra ClassDefn, not reachable via any path_elems chain the other tests
  // happen to check).

  template <typename T>
  static size_t countAll() {
    return m_session->getDatabase().getObjects<T>().size();
  }

  // Counts only the objects of type T whose own vpiIsSpecialization flag is set -- i.e. the
  // ones getOrCreateSpecialization() actually minted, not a base definition nor an unspecialized
  // clone that merely came along for free as part of cloning some OTHER definition's own whole
  // subtree (e.g. pkg_deep::Level1's own nested Level2/Level3 get cloned when Level1 itself is
  // specialized, but neither clone is itself is_specialization==true unless ALSO independently
  // given its own #(...) override at some use site).
  template <typename T>
  static size_t countSpecializations() {
    size_t n = 0;
    for (hldb::Any *const source : m_session->getDatabase().getObjects<T>()) {
      if (const T *const t = any_cast<T>(source)) {
        if (t->getIsSpecialization()) ++n;
      }
    }
    return n;
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
// Two independent references to overrides deep_handle above already uses (Task 10 fifth
// follow-up) -- regression coverage for a real duplicate-specialization bug: a scoped chain's
// own segment RefTypespec is ALSO independently reachable by resolveUnsupportedTypespecs()'s
// own flat sweep (Phase2 parents it directly under the enclosing chain, same as any other
// RefTypespec), and if that sweep resolves it before resolveScopedUnsupportedTypespec()'s own
// per-segment loop consumes it, the segment's own override is silently lost -- falling back to
// the UNSPECIALIZED base and corrupting every later segment's own search scope. Confirmed via
// -d db: Level2 ended up specialized TWICE for deep_handle's own chain alone (once correctly,
// nested under Level1's own specialization; once wrongly, nested under Level1's own base) --
// invisible to the tests above, since they only ever inspect the chain's own FINAL, correctly-
// dispatched path_elems, never the orphaned duplicate the race also produces along the way.
// level1_alias_handle/level2_alias_handle must dedupe to the EXACT SAME Level1/Level2
// specializations deep_handle's own chain already resolved to.
// =============================================================================================

TEST_F(StaticElaborationTest, Level1AliasHandleDedupesWithDeepHandlesOwnLevel1Segment) {
  const hldb::ClassTypespec *const deepCt = getHandleTypespec("deep_handle");
  ASSERT_NE(deepCt, nullptr);
  const hldb::ClassTypespec *const level1Ct = getPathElemClassTypespec(deepCt, 1);
  ASSERT_NE(level1Ct, nullptr);
  const hldb::ClassDefn *const expectedLevel1 = level1Ct->getClassDefn();
  ASSERT_NE(expectedLevel1, nullptr);

  const hldb::ClassTypespec *const aliasCt = getHandleTypespec("level1_alias_handle");
  ASSERT_NE(aliasCt, nullptr);
  const hldb::ClassDefn *const actualLevel1 = aliasCt->getClassDefn();
  ASSERT_NE(actualLevel1, nullptr);
  EXPECT_EQ(actualLevel1, expectedLevel1)
      << "pkg_deep::Level1#(11), referenced independently here, must resolve to the exact same "
         "specialization deep_handle's own chain already produced for the identical override -- "
         "not a second, separately-minted (and, per the bug this guards against, potentially "
         "wrongly-based) clone";
}

TEST_F(StaticElaborationTest, Level2AliasHandleDedupesWithDeepHandlesOwnLevel2Segment) {
  const hldb::ClassTypespec *const deepCt = getHandleTypespec("deep_handle");
  ASSERT_NE(deepCt, nullptr);
  const hldb::ClassTypespec *const level2Ct = getPathElemClassTypespec(deepCt, 2);
  ASSERT_NE(level2Ct, nullptr);
  const hldb::ClassDefn *const expectedLevel2 = level2Ct->getClassDefn();
  ASSERT_NE(expectedLevel2, nullptr);

  const hldb::ClassTypespec *const aliasCt = getHandleTypespec("level2_alias_handle");
  ASSERT_NE(aliasCt, nullptr);
  const hldb::ClassDefn *const actualLevel2 = aliasCt->getClassDefn();
  ASSERT_NE(actualLevel2, nullptr);
  EXPECT_EQ(actualLevel2, expectedLevel2)
      << "pkg_deep::Level1#(11)::Level2#(22), referenced independently here, must resolve its "
         "own Level2 segment to the exact same specialization deep_handle's own chain already "
         "produced -- this is the concrete case that failed before the fix: the duplicate was "
         "parented under Level1's own BASE (not its specialization), because the segment's own "
         "override had already been silently stolen by the flat sweep";

  // Also confirm the intermediate Level1 segment reached THIS WAY dedupes too -- same
  // specialization as both the direct level1_alias_handle test above and deep_handle's own
  // chain, regardless of which of the three independent references is resolved first.
  const hldb::ClassTypespec *const aliasLevel1Ct = getPathElemClassTypespec(aliasCt, 1);
  ASSERT_NE(aliasLevel1Ct, nullptr);
  const hldb::ClassTypespec *const deepLevel1Ct = getPathElemClassTypespec(deepCt, 1);
  ASSERT_NE(deepLevel1Ct, nullptr);
  EXPECT_EQ(aliasLevel1Ct->getClassDefn(), deepLevel1Ct->getClassDefn());
}

// =============================================================================================
// Typedefs in scope resolution (Task 10) plus the order-of-operations fix for resolving them
// (project_static_elaboration_plan's "Task 10 follow-up"). Three shapes: a typedef reached
// through a package scope (`pkg_td::pkg_alias_t` -- resolveScopedUnsupportedTypespec()'s own
// typedef fallback, landed but untested until now), a bare/unscoped typedef referenced with no
// `::` at all (`bare_alias_t` -- resolveUnsupportedTypespec()'s own flat-case typedef fallback,
// which previously had NO typedef awareness and would leave the reference permanently
// unresolved), and a typedef aliasing ANOTHER bare, unscoped typedef in the same scope
// (`chained_alias_t` -- a typedef-of-typedef chain, regression coverage for
// resolveUnsupportedTypespecs()'s fixed-point retry across two independent, mutually-adjacent
// RefTypespec entries in the same sweep).
//
// A typedef never carries its own #(...) override -- the override lives on, and is resolved
// through, the typedef's own underlying type (getTypedefAlias()) instead, so a typedef'd
// handle's own RefTypespec::actual is the TypedefTypespec itself, never dereferenced
// automatically; every test below follows getTypedefAlias() explicitly to reach the
// (possibly specialized) ClassTypespec underneath.
// =============================================================================================

TEST_F(StaticElaborationTest, PkgTdExistsWithItsOwnAlias) {
  const hldb::Package *const pkgTd = getPkgTd();
  ASSERT_NE(pkgTd, nullptr);
  ASSERT_NE(pkgTd->getTypespecs(), nullptr);
  bool found = false;
  for (const hldb::Typespec *const ts : *pkgTd->getTypespecs()) {
    if (const hldb::TypedefTypespec *const td = any_cast<hldb::TypedefTypespec>(ts)) {
      if (td->getName() == std::string_view("pkg_alias_t")) found = true;
    }
  }
  EXPECT_TRUE(found);
}

TEST_F(StaticElaborationTest, PkgAliasHandleResolvesThroughPackageScopedTypedef) {
  const hldb::TypedefTypespec *const td = getHandleTypedefTypespec("pkg_alias_handle");
  ASSERT_NE(td, nullptr) << "pkg_td::pkg_alias_t must bind to the TypedefTypespec itself via "
                             "resolveScopedUnsupportedTypespec()'s own typedef fallback, not "
                             "leave the reference as an unresolved UnsupportedTypespec";
  EXPECT_EQ(td->getName(), std::string_view("pkg_alias_t"));

  const hldb::ClassTypespec *const underlying = getTypedefAliasClassTypespec(td);
  ASSERT_NE(underlying, nullptr) << "the typedef's own underlying type (param_cls#(40)) must "
                                     "also resolve via the same sweep";
  const hldb::ClassDefn *const specialized = underlying->getClassDefn();
  ASSERT_NE(specialized, nullptr);
  EXPECT_NE(specialized, getParamCls()) << "param_cls#(40) must specialize, not bind to the base";
  const hldb::ParamAssign *const pa = getParamAssignByName(specialized, "W");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), std::string_view("40"));
}

TEST_F(StaticElaborationTest, BareAliasHandleResolvesThroughFlatCaseTypedefFallback) {
  const hldb::TypedefTypespec *const td = getHandleTypedefTypespec("bare_alias_handle");
  ASSERT_NE(td, nullptr) << "a bare, unqualified typedef reference (no '::' at all) must resolve "
                             "via resolveUnsupportedTypespec()'s own typedef fallback -- before "
                             "this fix, the flat resolver had no typedef awareness and this "
                             "would stay permanently unresolved regardless of sweep order";
  EXPECT_EQ(td->getName(), std::string_view("bare_alias_t"));

  const hldb::ClassTypespec *const underlying = getTypedefAliasClassTypespec(td);
  ASSERT_NE(underlying, nullptr);
  const hldb::ClassDefn *const specialized = underlying->getClassDefn();
  ASSERT_NE(specialized, nullptr);
  EXPECT_NE(specialized, getParamCls());
  const hldb::ParamAssign *const pa = getParamAssignByName(specialized, "W");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), std::string_view("41"));
}

TEST_F(StaticElaborationTest, ChainedAliasHandleResolvesThroughTypedefOfTypedef) {
  const hldb::TypedefTypespec *const chainedTd = getHandleTypedefTypespec("chained_alias_handle");
  ASSERT_NE(chainedTd, nullptr);
  EXPECT_EQ(chainedTd->getName(), std::string_view("chained_alias_t"));

  ASSERT_NE(chainedTd->getTypedefAlias(), nullptr);
  const hldb::TypedefTypespec *const bareTd = chainedTd->getTypedefAlias()->getActual<hldb::TypedefTypespec>();
  ASSERT_NE(bareTd, nullptr) << "chained_alias_t's own underlying type is bare_alias_t itself -- "
                                 "another typedef, not a class -- and must resolve to it via the "
                                 "same flat-case typedef fallback, not stay an unresolved "
                                 "UnsupportedTypespec";
  EXPECT_EQ(bareTd->getName(), std::string_view("bare_alias_t"));

  const hldb::ClassTypespec *const underlying = getTypedefAliasClassTypespec(bareTd);
  ASSERT_NE(underlying, nullptr) << "bare_alias_t's OWN underlying type (param_cls#(41)) must "
                                     "also be fully resolved -- regardless of which of these two "
                                     "independent RefTypespec entries the sweep happens to visit "
                                     "first";
  const hldb::ClassDefn *const specialized = underlying->getClassDefn();
  ASSERT_NE(specialized, nullptr);
  const hldb::ParamAssign *const pa = getParamAssignByName(specialized, "W");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), std::string_view("41"));
}

// =============================================================================================
// Typedef whose own underlying type is a specialized class with further-parameterized nested
// classes (Task 10 second follow-up) -- TdOuterAlias::TdMid#(81)::TdInner#(82). This is the
// concrete scenario a SINGLE PASS genuinely cannot resolve reliably: the segment-0 typedef
// fallback needs TdOuterAlias's own aliased class (TdOuter#(80)) to already be resolved AND
// SPECIALIZED before TdMid can even be searched for -- it must be looked up within
// TdOuter#(80)'s own cloned nested-class list, not TdOuter's base one -- and TdOuter#(80)'s own
// specialization is itself just another, independent RefTypespec entry in the SAME
// resolveUnsupportedTypespecs() sweep, resolved by resolveUnsupportedTypespec()'s own inline
// getOrCreateSpecialization() call. Whichever of these two entries the sweep happens to visit
// first, the other one must be retried once the first has actually finished -- exactly what
// the fixed-point retry provides and a single pass cannot guarantee.
// =============================================================================================

TEST_F(StaticElaborationTest, TdOuterTdMidTdInnerClassesExist) {
  const hldb::ClassDefn *const tdOuter = getModuleLocalClass(getDut(), "TdOuter");
  ASSERT_NE(tdOuter, nullptr);
  const hldb::ClassDefn *const tdMid = getNestedClass(tdOuter, "TdMid");
  ASSERT_NE(tdMid, nullptr);
  const hldb::ClassDefn *const tdInner = getNestedClass(tdMid, "TdInner");
  EXPECT_NE(tdInner, nullptr);
}

TEST_F(StaticElaborationTest, TdOuterAliasIsATypedefResolvingToSpecializedTdOuter) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getTypespecs(), nullptr);
  const hldb::TypedefTypespec *aliasTd = nullptr;
  for (const hldb::Typespec *const ts : *dut->getTypespecs()) {
    if (const hldb::TypedefTypespec *const td = any_cast<hldb::TypedefTypespec>(ts)) {
      if (td->getName() == std::string_view("TdOuterAlias")) aliasTd = td;
    }
  }
  ASSERT_NE(aliasTd, nullptr);

  const hldb::ClassTypespec *const underlying = getTypedefAliasClassTypespec(aliasTd);
  ASSERT_NE(underlying, nullptr) << "TdOuterAlias's own underlying type (TdOuter#(80)) must "
                                     "resolve -- and specialize -- via "
                                     "resolveUnsupportedTypespec()'s own inline "
                                     "getOrCreateSpecialization() call";
  const hldb::ClassDefn *const specializedOuter = underlying->getClassDefn();
  ASSERT_NE(specializedOuter, nullptr);
  EXPECT_NE(specializedOuter, getModuleLocalClass(getDut(), "TdOuter"));
  const hldb::ParamAssign *const pa = getParamAssignByName(specializedOuter, "W");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), std::string_view("80"));
}

TEST_F(StaticElaborationTest, TdChainHandleResolvesThroughSpecializedTypedefLeadingSegment) {
  const hldb::ClassTypespec *const ct = getHandleTypespec("td_chain_handle");
  ASSERT_NE(ct, nullptr) << "TdOuterAlias::TdMid#(81)::TdInner#(82) must fully resolve -- a "
                             "single sweep pass is not enough in general for this shape (see "
                             "the section comment above); this is the concrete regression case "
                             "for resolveUnsupportedTypespecs()'s fixed-point retry";
  ASSERT_NE(ct->getPathElems(), nullptr);
  ASSERT_EQ(ct->getPathElems()->size(), 3u);

  const hldb::ClassDefn *const specializedInner = ct->getClassDefn();
  ASSERT_NE(specializedInner, nullptr);
  const hldb::ParamAssign *const pa = getParamAssignByName(specializedInner, "W");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), std::string_view("82"));
}

TEST_F(StaticElaborationTest, TdChainHandleMidSegmentIsSpecializedWithinTdOuterAliasSpecialization) {
  const hldb::ClassTypespec *const ct = getHandleTypespec("td_chain_handle");
  ASSERT_NE(ct, nullptr);
  // path_elems: [0] = TdOuterAlias (a typedef, not a class -- getPathElemClassTypespec doesn't
  // apply to it, see TdOuterAliasIsATypedefResolvingToSpecializedTdOuter instead), [1] = TdMid,
  // [2] = TdInner (== ct itself).
  const hldb::ClassTypespec *const midCt = getPathElemClassTypespec(ct, 1);
  ASSERT_NE(midCt, nullptr);
  const hldb::ClassDefn *const specializedMid = midCt->getClassDefn();
  ASSERT_NE(specializedMid, nullptr);

  const hldb::ClassDefn *const baseTdOuter = getModuleLocalClass(getDut(), "TdOuter");
  const hldb::ClassDefn *const baseTdMid = getNestedClass(baseTdOuter, "TdMid");
  ASSERT_NE(baseTdMid, nullptr);
  EXPECT_NE(specializedMid, baseTdMid) << "TdMid must be looked up (and re-specialized) within "
                                          "TdOuterAlias's own specialization's nested class "
                                          "list, not TdOuter's base one";

  const hldb::ParamAssign *const pa = getParamAssignByName(specializedMid, "W");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), std::string_view("81"));
}

// =============================================================================================
// Lexical (walk-up) scoping for a bare, unqualified typedef reference (Task 10 third
// follow-up) -- Phase3ModelBuilder::findTypedefInEnclosingScopes(). A `::`-qualified segment is
// bound EXACTLY to its resolved scope (see the tests above), but an unqualified name follows
// ordinary SV name resolution: visible in its own immediate scope, then every scope enclosing
// that, all the way out to file/compilation-unit ("$unit") scope. Two shapes: a typedef
// declared on a class, referenced bare from a NESTED class scope (TdMid seeing TdOuter's own
// outer_scope_alias_t), and a typedef declared outside any module/package/class at all,
// referenced bare from deep inside dut (walking all the way out to Design itself, which is NOT
// a Scope but still holds its own top-level typedefs field for exactly this case).
// =============================================================================================

TEST_F(StaticElaborationTest, ScopeWalkMemberResolvesTypedefFromEnclosingClassScope) {
  const hldb::ClassDefn *const tdOuter = getModuleLocalClass(getDut(), "TdOuter");
  ASSERT_NE(tdOuter, nullptr);
  const hldb::ClassDefn *const tdMid = getNestedClass(tdOuter, "TdMid");
  ASSERT_NE(tdMid, nullptr);

  const hldb::TypedefTypespec *const td = getMemberTypedefTypespec(tdMid, "scope_walk_member");
  ASSERT_NE(td, nullptr) << "outer_scope_alias_t is declared on TdOuter (TdMid's own enclosing "
                             "scope), not on TdMid itself -- resolving a bare reference to it "
                             "from within TdMid requires findTypedefInEnclosingScopes() to walk "
                             "UP the parent chain, not just check TdMid's own immediate scope";
  EXPECT_EQ(td->getName(), std::string_view("outer_scope_alias_t"));

  const hldb::ClassTypespec *const underlying = getTypedefAliasClassTypespec(td);
  ASSERT_NE(underlying, nullptr);
  const hldb::ClassDefn *const specialized = underlying->getClassDefn();
  ASSERT_NE(specialized, nullptr);
  const hldb::ParamAssign *const pa = getParamAssignByName(specialized, "W");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), std::string_view("95"));
}

TEST_F(StaticElaborationTest, UnitScopeHandleResolvesTypedefWalkedAllTheWayToDesign) {
  const hldb::TypedefTypespec *const td = getHandleTypedefTypespec("unit_scope_handle");
  ASSERT_NE(td, nullptr) << "unit_scope_alias_t is declared outside any module/package/class -- "
                             "parented directly under Design, not under any Scope -- so "
                             "resolving a bare reference to it from inside dut requires "
                             "findTypedefInEnclosingScopes() to walk all the way out to Design's "
                             "own explicit termination case";
  EXPECT_EQ(td->getName(), std::string_view("unit_scope_alias_t"));

  const hldb::ClassTypespec *const underlying = getTypedefAliasClassTypespec(td);
  ASSERT_NE(underlying, nullptr);
  const hldb::ClassDefn *const specialized = underlying->getClassDefn();
  ASSERT_NE(specialized, nullptr);
  const hldb::ParamAssign *const pa = getParamAssignByName(specialized, "W");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), std::string_view("96"));
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

// =============================================================================================
// Whole-graph object counts. A coarser, complementary check to every dedup/identity assertion
// above: those each confirm a handful of SPECIFIC use sites resolve to the right (shared, or
// correctly distinct) object, but none of them notices a stray EXTRA object sitting elsewhere
// in the database, unreachable from any of the specific paths they happen to check -- exactly
// the shape of the orphaned, wrongly-based duplicate ClassDefns Task 10's fifth follow-up found
// (see project_static_elaboration_plan): a second "Level2" specialization existed, parented
// under Level1's own BASE instead of its specialization, invisible to every test that only
// ever walks the correctly-dispatched chain's own path_elems. An unexpected total count here
// would have caught that directly, with no need to already know which specific object to check.
//
// Every number below was derived by hand from dut.sv's own declarations, walking through
// EVERY override-bearing use site and its own getOrCreateSpecialization() call (documented
// inline per definition kind) -- NOT read back from a prior run's own dump (which would just
// re-lock in whatever the tool happened to produce, bug included, exactly the anti-pattern
// davtests.md itself warns against). If a count below ever needs to change, re-derive it by
// hand against dut.sv's own current declarations first -- never adjust it to match a dump
// without first confirming the ADDITIONAL declaration/use-site that justifies the new number.
// =============================================================================================

// ---- Module: 3 base (param_mod, mid_level, dut) + 2 specializations (param_mod#16, shared by
// inst_wide and mid_level's own inst_nested_same_as_wide; param_mod#24, mid_level's own
// inst_nested_distinct) = 5 total. param_mod has no nested module declarations of its own, so
// specializing it clones nothing further. mid_level/dut carry no parameters, so neither is ever
// itself specialized (module INSTANTIATION, unlike a nested class DECLARATION, never mints a
// new Module ClassDefn-equivalent of its own).

TEST_F(StaticElaborationTest, TotalModuleCountInGraph) { EXPECT_EQ(countAll<hldb::Module>(), 5u); }

TEST_F(StaticElaborationTest, TotalModuleSpecializationCount) {
  EXPECT_EQ(countSpecializations<hldb::Module>(), 2u);
}

// ---- Interface: 1 base (param_if) + 1 specialization (param_if#8, if_wide) = 2 total.

TEST_F(StaticElaborationTest, TotalInterfaceCountInGraph) { EXPECT_EQ(countAll<hldb::Interface>(), 2u); }

TEST_F(StaticElaborationTest, TotalInterfaceSpecializationCount) {
  EXPECT_EQ(countSpecializations<hldb::Interface>(), 1u);
}

// ---- Program: 1 base (param_prog) + 1 specialization (param_prog#4, prog_wide) = 2 total.

TEST_F(StaticElaborationTest, TotalProgramCountInGraph) { EXPECT_EQ(countAll<hldb::Program>(), 2u); }

TEST_F(StaticElaborationTest, TotalProgramSpecializationCount) {
  EXPECT_EQ(countSpecializations<hldb::Program>(), 1u);
}

// ---- Package: pkg_a, pkg_b, pkg_deep, pkg_td = 4 total. Packages have no parameter mechanism
// at all in this model -- never specialized, so no companion specialization-count test.

TEST_F(StaticElaborationTest, TotalPackageCountInGraph) { EXPECT_EQ(countAll<hldb::Package>(), 4u); }

// ---- ClassDefn: the complex one -- 13 base declarations, 15 specializations (one per distinct
// (baseDef, override) pair actually requested anywhere in the file), and 6 further,
// UNSPECIALIZED clones that come along for free as a byproduct of cloning a WHOLE subtree
// whenever ITS OWN enclosing class gets specialized (a clone is only counted as one of the 15
// "specializations" above if it is ITSELF the direct target of its own getOrCreateSpecialization()
// call -- an intermediate clone reached only by walking INTO an already-specialized parent is
// not). 13 + 15 + 6 = 34 total.
//
// Base declarations (13): param_cls; pkg_a::PkgWidget; pkg_b::PkgWidget; OuterCls;
// OuterCls::InnerCls; OtherOuterCls; OtherOuterCls::InnerCls; pkg_deep::Level1;
// pkg_deep::Level1::Level2; pkg_deep::Level1::Level2::Level3; TdOuter; TdOuter::TdMid;
// TdOuter::TdMid::TdInner.
//
// Specializations (15), grouped by base:
//   param_cls -- 5 distinct override values requested across the file: #(9) (cls_wide_handle),
//     #(40) (pkg_alias_t's own underlying type), #(41) (bare_alias_t's own underlying type --
//     chained_alias_t aliases bare_alias_t itself, not param_cls directly, so it dedupes to
//     this SAME specialization rather than adding a 6th), #(95) (outer_scope_alias_t's own
//     underlying type), #(96) (unit_scope_alias_t's own underlying type). None of these clone
//     anything further -- param_cls has no nested classes.
//   pkg_a::PkgWidget -- #(10) (pkg_a_handle). pkg_b::PkgWidget -- #(20) (pkg_b_handle). Two
//     DIFFERENT bases (different packages), so two separate specializations despite the shared
//     name and shared override VALUE -- confirmed already by PkgAHandle.../PkgBHandle... above.
//   OuterCls::InnerCls -- #(12) (outer_inner_handle). OtherOuterCls::InnerCls -- #(24)
//     (other_outer_inner_handle). Same shape as PkgWidget above.
//   pkg_deep's own 3-level chain -- Level1#(11) (shared by deep_handle, level1_alias_handle,
//     level2_alias_handle); Level2#(22), specializing the CLONE of Level2 that lives inside
//     Level1#(11)'s own clone (shared by deep_handle, level2_alias_handle); Level3#(33),
//     specializing the clone of Level3 that lives inside THAT Level2#(22) specialization
//     (deep_handle only) = 3 specializations.
//   TdOuter's own 3-level chain, structurally identical to pkg_deep's -- TdOuter#(80) (via
//     TdOuterAlias's own underlying type); TdMid#(81) (td_chain_handle); TdInner#(82)
//     (td_chain_handle) = 3 specializations.
//   5 + 1 + 1 + 1 + 1 + 3 + 3 = 15.
//
// Unspecialized transitive clones (6): Level1#(11)'s own clone step clones Level2 AND (nested
// within that) Level3 = 2; Level2#(22)'s own clone step clones Level3 (a SECOND, independent
// Level3 clone, nested inside the Level2 specialization rather than inside the bare Level1
// clone) = 1; Level3#(33) has no nested classes to clone = 0. Same shape for TdOuter's own
// chain: TdOuter#(80) clones TdMid and (nested) TdInner = 2; TdMid#(81) clones TdInner again
// (nested inside the TdMid specialization) = 1; TdInner#(82) clones nothing = 0.
// 2 + 1 + 0 + 2 + 1 + 0 = 6.

TEST_F(StaticElaborationTest, TotalClassDefnCountInGraph) { EXPECT_EQ(countAll<hldb::ClassDefn>(), 34u); }

TEST_F(StaticElaborationTest, TotalClassDefnSpecializationCount) {
  EXPECT_EQ(countSpecializations<hldb::ClassDefn>(), 15u);
}

// ---- TypedefTypespec: pkg_alias_t, unit_scope_alias_t, bare_alias_t, chained_alias_t,
// outer_scope_alias_t, TdOuterAlias = 6 total. Confirmed never cloned regardless of how many
// times their own enclosing scope gets specialized -- Phase3Cloner explicitly shares (never
// duplicates) a TypedefTypespec, exactly like every other "reference-shaped" typespec kind, so
// this count is stable and never multiplied by specialization the way ClassDefn's is.

TEST_F(StaticElaborationTest, TotalTypedefTypespecCountInGraph) {
  EXPECT_EQ(countAll<hldb::TypedefTypespec>(), 6u);
}

// ---- Sanity check: every UnsupportedTypespec Phase2 ever built as a placeholder for this file
// should have been resolved (and orphaned -- setParent(nullptr), per this file's own GC
// convention) by the time Phase3 finishes. A non-zero count here means something was left
// unresolved -- exactly the failure mode ObjectBinder's own scope-unaware findType() fallback
// would otherwise silently paper over (see Task 10's fifth follow-up for a concrete case where
// that fallback masked a real Phase3 bug). Deliberately checks "still has a live parent", not
// "count is zero" -- the ORIGINAL placeholder objects still exist (orphaned, not destroyed, per
// this codebase's leave-orphans-to-GC convention), so a raw total count would never reach zero.

TEST_F(StaticElaborationTest, NoUnresolvedScopedTypespecsRemain) {
  size_t stillLive = 0;
  for (hldb::Any *const source : m_session->getDatabase().getObjects<hldb::UnsupportedTypespec>()) {
    if (const hldb::UnsupportedTypespec *const ut = any_cast<hldb::UnsupportedTypespec>(source)) {
      if (ut->getParent() != nullptr) ++stillLive;
    }
  }
  EXPECT_EQ(stillLive, 0u);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
