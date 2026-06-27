// -*- c++ -*-

/*

 Copyright 2019 Alain Dargelas

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

/*
 * File:   UhdmVisitor.h
 * Author: hs
 *
 * Created on October 01, 2025, 00:00 AM
 */

#ifndef UHDM_UHDMVISITOR_H
#define UHDM_UHDMVISITOR_H

#include <uhdm/containers.h>
#include <uhdm/sv_vpi_user.h>
#include <uhdm/uhdm_types.h>

namespace uhdm {
class Serializer;

class UhdmVisitor {
 protected:
  using any_stack_t = std::vector<const Any *>;

public:
  // Use implicit constructor to initialize all members
  // UhdmVisitor()
  virtual ~UhdmVisitor() = default;

public:
  AnySet &getVisited() { return m_visited; }
  const AnySet &getVisited() const { return m_visited; }

  void requestAbort() { m_abortRequested = true; }

  bool didVisitAll(const Serializer &serializer) const;
  void visit(const Any *object);

  // clang-format off
  virtual void visitAny(const Any* object) {}
  virtual void visitAlias(const Alias* object) {}
  virtual void visitAlways(const Always* object) {}
  virtual void visitAnyPattern(const AnyPattern* object) {}
  virtual void visitArrayExpr(const ArrayExpr* object) {}
  virtual void visitArrayTypespec(const ArrayTypespec* object) {}
  virtual void visitAssert(const Assert* object) {}
  virtual void visitAssignStmt(const AssignStmt* object) {}
  virtual void visitAssignment(const Assignment* object) {}
  virtual void visitAssume(const Assume* object) {}
  virtual void visitAttribute(const Attribute* object) {}
  virtual void visitBegin(const Begin* object) {}
  virtual void visitBindDirective(const BindDirective* object) {}
  virtual void visitBitSelect(const BitSelect* object) {}
  virtual void visitBitTypespec(const BitTypespec* object) {}
  virtual void visitBreakStmt(const BreakStmt* object) {}
  virtual void visitByteTypespec(const ByteTypespec* object) {}
  virtual void visitCaseItem(const CaseItem* object) {}
  virtual void visitCaseProperty(const CaseProperty* object) {}
  virtual void visitCasePropertyItem(const CasePropertyItem* object) {}
  virtual void visitCaseStmt(const CaseStmt* object) {}
  virtual void visitChandleTypespec(const ChandleTypespec* object) {}
  virtual void visitCheckerDecl(const CheckerDecl* object) {}
  virtual void visitCheckerInst(const CheckerInst* object) {}
  virtual void visitCheckerInstPort(const CheckerInstPort* object) {}
  virtual void visitCheckerPort(const CheckerPort* object) {}
  virtual void visitClassDefn(const ClassDefn* object) {}
  virtual void visitClassObj(const ClassObj* object) {}
  virtual void visitClassTypespec(const ClassTypespec* object) {}
  virtual void visitClause(const Clause* object) {}
  virtual void visitClockedProperty(const ClockedProperty* object) {}
  virtual void visitClockedSeq(const ClockedSeq* object) {}
  virtual void visitClockingBlock(const ClockingBlock* object) {}
  virtual void visitClockingIODecl(const ClockingIODecl* object) {}
  virtual void visitComment(const Comment* object) {}
  virtual void visitConfigDecl(const ConfigDecl* object) {}
  virtual void visitConfigRule(const ConfigRule* object) {}
  virtual void visitConstant(const Constant* object) {}
  virtual void visitConstrForeach(const ConstrForeach* object) {}
  virtual void visitConstrIf(const ConstrIf* object) {}
  virtual void visitConstrIfElse(const ConstrIfElse* object) {}
  virtual void visitConstraint(const Constraint* object) {}
  virtual void visitConstraintOrdering(const ConstraintOrdering* object) {}
  virtual void visitContAssign(const ContAssign* object) {}
  virtual void visitContAssignBit(const ContAssignBit* object) {}
  virtual void visitContinueStmt(const ContinueStmt* object) {}
  virtual void visitCover(const Cover* object) {}
  virtual void visitCoverBin(const CoverBin* object) {}
  virtual void visitCoverCross(const CoverCross* object) {}
  virtual void visitCoverGroup(const CoverGroup* object) {}
  virtual void visitCoverPoint(const CoverPoint* object) {}
  virtual void visitCoverageOption(const CoverageOption* object) {}
  virtual void visitDeassign(const Deassign* object) {}
  virtual void visitDefParam(const DefParam* object) {}
  virtual void visitDelayControl(const DelayControl* object) {}
  virtual void visitDelayTerm(const DelayTerm* object) {}
  virtual void visitDesign(const Design* object) {}
  virtual void visitDisable(const Disable* object) {}
  virtual void visitDisableFork(const DisableFork* object) {}
  virtual void visitDistItem(const DistItem* object) {}
  virtual void visitDistribution(const Distribution* object) {}
  virtual void visitDoWhile(const DoWhile* object) {}
  virtual void visitEnumConst(const EnumConst* object) {}
  virtual void visitEnumTypespec(const EnumTypespec* object) {}
  virtual void visitEventControl(const EventControl* object) {}
  virtual void visitEventStmt(const EventStmt* object) {}
  virtual void visitEventTypespec(const EventTypespec* object) {}
  virtual void visitExpectStmt(const ExpectStmt* object) {}
  virtual void visitExtends(const Extends* object) {}
  virtual void visitFinalStmt(const FinalStmt* object) {}
  virtual void visitForStmt(const ForStmt* object) {}
  virtual void visitForce(const Force* object) {}
  virtual void visitForeachStmt(const ForeachStmt* object) {}
  virtual void visitForeverStmt(const ForeverStmt* object) {}
  virtual void visitForkStmt(const ForkStmt* object) {}
  virtual void visitFuncCall(const FuncCall* object) {}
  virtual void visitFunction(const Function* object) {}
  virtual void visitFunctionDecl(const FunctionDecl* object) {}
  virtual void visitGate(const Gate* object) {}
  virtual void visitGateArray(const GateArray* object) {}
  virtual void visitGenCase(const GenCase* object) {}
  virtual void visitGenFor(const GenFor* object) {}
  virtual void visitGenIf(const GenIf* object) {}
  virtual void visitGenIfElse(const GenIfElse* object) {}
  virtual void visitGenRegion(const GenRegion* object) {}
  virtual void visitGenScopeArray(const GenScopeArray* object) {}
  virtual void visitHierPath(const HierPath* object) {}
  virtual void visitIODecl(const IODecl* object) {}
  virtual void visitIdentifier(const Identifier* object) {}
  virtual void visitIfElse(const IfElse* object) {}
  virtual void visitIfStmt(const IfStmt* object) {}
  virtual void visitImmediateAssert(const ImmediateAssert* object) {}
  virtual void visitImmediateAssume(const ImmediateAssume* object) {}
  virtual void visitImmediateCover(const ImmediateCover* object) {}
  virtual void visitImplements(const Implements* object) {}
  virtual void visitImplication(const Implication* object) {}
  virtual void visitImportTypespec(const ImportTypespec* object) {}
  virtual void visitIncludeStmt(const IncludeStmt* object) {}
  virtual void visitIndexedPartSelect(const IndexedPartSelect* object) {}
  virtual void visitInitial(const Initial* object) {}
  virtual void visitIntTypespec(const IntTypespec* object) {}
  virtual void visitIntegerTypespec(const IntegerTypespec* object) {}
  virtual void visitInterface(const Interface* object) {}
  virtual void visitInterfaceArray(const InterfaceArray* object) {}
  virtual void visitInterfaceTFDecl(const InterfaceTFDecl* object) {}
  virtual void visitInterfaceTypespec(const InterfaceTypespec* object) {}
  virtual void visitLetDecl(const LetDecl* object) {}
  virtual void visitLetExpr(const LetExpr* object) {}
  virtual void visitLibrary(const Library* object) {}
  virtual void visitLogicTypespec(const LogicTypespec* object) {}
  virtual void visitLongIntTypespec(const LongIntTypespec* object) {}
  virtual void visitMethodFuncCall(const MethodFuncCall* object) {}
  virtual void visitMethodTaskCall(const MethodTaskCall* object) {}
  virtual void visitModPath(const ModPath* object) {}
  virtual void visitModport(const Modport* object) {}
  virtual void visitModule(const Module* object) {}
  virtual void visitModuleArray(const ModuleArray* object) {}
  virtual void visitModuleTypespec(const ModuleTypespec* object) {}
  virtual void visitMulticlockSequenceExpr(const MulticlockSequenceExpr* object) {}
  virtual void visitNamedEvent(const NamedEvent* object) {}
  virtual void visitNamedEventArray(const NamedEventArray* object) {}
  virtual void visitNet(const Net* object) {}
  virtual void visitNullStmt(const NullStmt* object) {}
  virtual void visitOperation(const Operation* object) {}
  virtual void visitOrderedWait(const OrderedWait* object) {}
  virtual void visitPackage(const Package* object) {}
  virtual void visitPackageTypespec(const PackageTypespec* object) {}
  virtual void visitParamAssign(const ParamAssign* object) {}
  virtual void visitParameter(const Parameter* object) {}
  virtual void visitPartSelect(const PartSelect* object) {}
  virtual void visitPathTerm(const PathTerm* object) {}
  virtual void visitPort(const Port* object) {}
  virtual void visitPortBit(const PortBit* object) {}
  virtual void visitPreprocMacroDefinition(const PreprocMacroDefinition* object) {}
  virtual void visitPreprocMacroInstance(const PreprocMacroInstance* object) {}
  virtual void visitPrimTerm(const PrimTerm* object) {}
  virtual void visitProgram(const Program* object) {}
  virtual void visitProgramArray(const ProgramArray* object) {}
  virtual void visitProgramTypespec(const ProgramTypespec* object) {}
  virtual void visitPropFormalDecl(const PropFormalDecl* object) {}
  virtual void visitPropertyDecl(const PropertyDecl* object) {}
  virtual void visitPropertyInst(const PropertyInst* object) {}
  virtual void visitPropertySpec(const PropertySpec* object) {}
  virtual void visitPropertyTypespec(const PropertyTypespec* object) {}
  virtual void visitRange(const Range* object) {}
  virtual void visitRealTypespec(const RealTypespec* object) {}
  virtual void visitRefInstance(const RefInstance* object) {}
  virtual void visitRefObj(const RefObj* object) {}
  virtual void visitRefTypespec(const RefTypespec* object) {}
  virtual void visitReg(const Reg* object) {}
  virtual void visitRegArray(const RegArray* object) {}
  virtual void visitRelease(const Release* object) {}
  virtual void visitRepeat(const Repeat* object) {}
  virtual void visitRepeatControl(const RepeatControl* object) {}
  virtual void visitRestrict(const Restrict* object) {}
  virtual void visitReturnStmt(const ReturnStmt* object) {}
  virtual void visitSeqFormalDecl(const SeqFormalDecl* object) {}
  virtual void visitSequenceDecl(const SequenceDecl* object) {}
  virtual void visitSequenceInst(const SequenceInst* object) {}
  virtual void visitSequenceTypespec(const SequenceTypespec* object) {}
  virtual void visitShortIntTypespec(const ShortIntTypespec* object) {}
  virtual void visitShortRealTypespec(const ShortRealTypespec* object) {}
  virtual void visitSoftDisable(const SoftDisable* object) {}
  virtual void visitSourceFile(const SourceFile* object) {}
  virtual void visitSpecParam(const SpecParam* object) {}
  virtual void visitStringTypespec(const StringTypespec* object) {}
  virtual void visitStructPattern(const StructPattern* object) {}
  virtual void visitStructTypespec(const StructTypespec* object) {}
  virtual void visitSwitchArray(const SwitchArray* object) {}
  virtual void visitSwitchTran(const SwitchTran* object) {}
  virtual void visitSysFuncCall(const SysFuncCall* object) {}
  virtual void visitSysTaskCall(const SysTaskCall* object) {}
  virtual void visitTableEntry(const TableEntry* object) {}
  virtual void visitTaggedPattern(const TaggedPattern* object) {}
  virtual void visitTask(const Task* object) {}
  virtual void visitTaskCall(const TaskCall* object) {}
  virtual void visitTaskDecl(const TaskDecl* object) {}
  virtual void visitTchk(const Tchk* object) {}
  virtual void visitTchkTerm(const TchkTerm* object) {}
  virtual void visitThread(const Thread* object) {}
  virtual void visitTimeTypespec(const TimeTypespec* object) {}
  virtual void visitTypeParameter(const TypeParameter* object) {}
  virtual void visitTypedefTypespec(const TypedefTypespec* object) {}
  virtual void visitTypespecMember(const TypespecMember* object) {}
  virtual void visitUdp(const Udp* object) {}
  virtual void visitUdpArray(const UdpArray* object) {}
  virtual void visitUdpDefn(const UdpDefn* object) {}
  virtual void visitUdpDefnTypespec(const UdpDefnTypespec* object) {}
  virtual void visitUnionTypespec(const UnionTypespec* object) {}
  virtual void visitUniqueness(const Uniqueness* object) {}
  virtual void visitUnsupportedExpr(const UnsupportedExpr* object) {}
  virtual void visitUnsupportedStmt(const UnsupportedStmt* object) {}
  virtual void visitUnsupportedTypespec(const UnsupportedTypespec* object) {}
  virtual void visitUserSystf(const UserSystf* object) {}
  virtual void visitVarSelect(const VarSelect* object) {}
  virtual void visitVariable(const Variable* object) {}
  virtual void visitVoidTypespec(const VoidTypespec* object) {}
  virtual void visitWaitFork(const WaitFork* object) {}
  virtual void visitWaitStmt(const WaitStmt* object) {}
  virtual void visitWhileStmt(const WhileStmt* object) {}

  virtual void visitAliasCollection(const Any* object, const AliasCollection& objects) {}
  virtual void visitAlwaysCollection(const Any* object, const AlwaysCollection& objects) {}
  virtual void visitAnyCollection(const Any* object, const AnyCollection& objects) {}
  virtual void visitAnyPatternCollection(const Any* object, const AnyPatternCollection& objects) {}
  virtual void visitArrayExprCollection(const Any* object, const ArrayExprCollection& objects) {}
  virtual void visitArrayTypespecCollection(const Any* object, const ArrayTypespecCollection& objects) {}
  virtual void visitAssertCollection(const Any* object, const AssertCollection& objects) {}
  virtual void visitAssignStmtCollection(const Any* object, const AssignStmtCollection& objects) {}
  virtual void visitAssignmentCollection(const Any* object, const AssignmentCollection& objects) {}
  virtual void visitAssumeCollection(const Any* object, const AssumeCollection& objects) {}
  virtual void visitAtomicStmtCollection(const Any* object, const AtomicStmtCollection& objects) {}
  virtual void visitAttributeCollection(const Any* object, const AttributeCollection& objects) {}
  virtual void visitBeginCollection(const Any* object, const BeginCollection& objects) {}
  virtual void visitBindDirectiveCollection(const Any* object, const BindDirectiveCollection& objects) {}
  virtual void visitBitSelectCollection(const Any* object, const BitSelectCollection& objects) {}
  virtual void visitBitTypespecCollection(const Any* object, const BitTypespecCollection& objects) {}
  virtual void visitBreakStmtCollection(const Any* object, const BreakStmtCollection& objects) {}
  virtual void visitByteTypespecCollection(const Any* object, const ByteTypespecCollection& objects) {}
  virtual void visitCaseItemCollection(const Any* object, const CaseItemCollection& objects) {}
  virtual void visitCasePropertyCollection(const Any* object, const CasePropertyCollection& objects) {}
  virtual void visitCasePropertyItemCollection(const Any* object, const CasePropertyItemCollection& objects) {}
  virtual void visitCaseStmtCollection(const Any* object, const CaseStmtCollection& objects) {}
  virtual void visitChandleTypespecCollection(const Any* object, const ChandleTypespecCollection& objects) {}
  virtual void visitCheckerDeclCollection(const Any* object, const CheckerDeclCollection& objects) {}
  virtual void visitCheckerInstCollection(const Any* object, const CheckerInstCollection& objects) {}
  virtual void visitCheckerInstPortCollection(const Any* object, const CheckerInstPortCollection& objects) {}
  virtual void visitCheckerPortCollection(const Any* object, const CheckerPortCollection& objects) {}
  virtual void visitClassDefnCollection(const Any* object, const ClassDefnCollection& objects) {}
  virtual void visitClassObjCollection(const Any* object, const ClassObjCollection& objects) {}
  virtual void visitClassTypespecCollection(const Any* object, const ClassTypespecCollection& objects) {}
  virtual void visitClauseCollection(const Any* object, const ClauseCollection& objects) {}
  virtual void visitClockedPropertyCollection(const Any* object, const ClockedPropertyCollection& objects) {}
  virtual void visitClockedSeqCollection(const Any* object, const ClockedSeqCollection& objects) {}
  virtual void visitClockingBlockCollection(const Any* object, const ClockingBlockCollection& objects) {}
  virtual void visitClockingIODeclCollection(const Any* object, const ClockingIODeclCollection& objects) {}
  virtual void visitCommentCollection(const Any* object, const CommentCollection& objects) {}
  virtual void visitConcurrentAssertionsCollection(const Any* object, const ConcurrentAssertionsCollection& objects) {}
  virtual void visitConfigDeclCollection(const Any* object, const ConfigDeclCollection& objects) {}
  virtual void visitConfigRuleCollection(const Any* object, const ConfigRuleCollection& objects) {}
  virtual void visitConstantCollection(const Any* object, const ConstantCollection& objects) {}
  virtual void visitConstrForeachCollection(const Any* object, const ConstrForeachCollection& objects) {}
  virtual void visitConstrIfCollection(const Any* object, const ConstrIfCollection& objects) {}
  virtual void visitConstrIfElseCollection(const Any* object, const ConstrIfElseCollection& objects) {}
  virtual void visitConstraintCollection(const Any* object, const ConstraintCollection& objects) {}
  virtual void visitConstraintExprCollection(const Any* object, const ConstraintExprCollection& objects) {}
  virtual void visitConstraintOrderingCollection(const Any* object, const ConstraintOrderingCollection& objects) {}
  virtual void visitContAssignCollection(const Any* object, const ContAssignCollection& objects) {}
  virtual void visitContAssignBitCollection(const Any* object, const ContAssignBitCollection& objects) {}
  virtual void visitContinueStmtCollection(const Any* object, const ContinueStmtCollection& objects) {}
  virtual void visitCoverCollection(const Any* object, const CoverCollection& objects) {}
  virtual void visitCoverBinCollection(const Any* object, const CoverBinCollection& objects) {}
  virtual void visitCoverCrossCollection(const Any* object, const CoverCrossCollection& objects) {}
  virtual void visitCoverGroupCollection(const Any* object, const CoverGroupCollection& objects) {}
  virtual void visitCoverPointCollection(const Any* object, const CoverPointCollection& objects) {}
  virtual void visitCoverageOptionCollection(const Any* object, const CoverageOptionCollection& objects) {}
  virtual void visitDeassignCollection(const Any* object, const DeassignCollection& objects) {}
  virtual void visitDefParamCollection(const Any* object, const DefParamCollection& objects) {}
  virtual void visitDelayControlCollection(const Any* object, const DelayControlCollection& objects) {}
  virtual void visitDelayTermCollection(const Any* object, const DelayTermCollection& objects) {}
  virtual void visitDesignCollection(const Any* object, const DesignCollection& objects) {}
  virtual void visitDisableCollection(const Any* object, const DisableCollection& objects) {}
  virtual void visitDisableForkCollection(const Any* object, const DisableForkCollection& objects) {}
  virtual void visitDisablesCollection(const Any* object, const DisablesCollection& objects) {}
  virtual void visitDistItemCollection(const Any* object, const DistItemCollection& objects) {}
  virtual void visitDistributionCollection(const Any* object, const DistributionCollection& objects) {}
  virtual void visitDoWhileCollection(const Any* object, const DoWhileCollection& objects) {}
  virtual void visitEnumConstCollection(const Any* object, const EnumConstCollection& objects) {}
  virtual void visitEnumTypespecCollection(const Any* object, const EnumTypespecCollection& objects) {}
  virtual void visitEventControlCollection(const Any* object, const EventControlCollection& objects) {}
  virtual void visitEventStmtCollection(const Any* object, const EventStmtCollection& objects) {}
  virtual void visitEventTypespecCollection(const Any* object, const EventTypespecCollection& objects) {}
  virtual void visitExpectStmtCollection(const Any* object, const ExpectStmtCollection& objects) {}
  virtual void visitExprCollection(const Any* object, const ExprCollection& objects) {}
  virtual void visitExtendsCollection(const Any* object, const ExtendsCollection& objects) {}
  virtual void visitFinalStmtCollection(const Any* object, const FinalStmtCollection& objects) {}
  virtual void visitForStmtCollection(const Any* object, const ForStmtCollection& objects) {}
  virtual void visitForceCollection(const Any* object, const ForceCollection& objects) {}
  virtual void visitForeachStmtCollection(const Any* object, const ForeachStmtCollection& objects) {}
  virtual void visitForeverStmtCollection(const Any* object, const ForeverStmtCollection& objects) {}
  virtual void visitForkStmtCollection(const Any* object, const ForkStmtCollection& objects) {}
  virtual void visitFuncCallCollection(const Any* object, const FuncCallCollection& objects) {}
  virtual void visitFunctionCollection(const Any* object, const FunctionCollection& objects) {}
  virtual void visitFunctionDeclCollection(const Any* object, const FunctionDeclCollection& objects) {}
  virtual void visitGateCollection(const Any* object, const GateCollection& objects) {}
  virtual void visitGateArrayCollection(const Any* object, const GateArrayCollection& objects) {}
  virtual void visitGenCaseCollection(const Any* object, const GenCaseCollection& objects) {}
  virtual void visitGenForCollection(const Any* object, const GenForCollection& objects) {}
  virtual void visitGenIfCollection(const Any* object, const GenIfCollection& objects) {}
  virtual void visitGenIfElseCollection(const Any* object, const GenIfElseCollection& objects) {}
  virtual void visitGenRegionCollection(const Any* object, const GenRegionCollection& objects) {}
  virtual void visitGenScopeCollection(const Any* object, const GenScopeCollection& objects) {}
  virtual void visitGenScopeArrayCollection(const Any* object, const GenScopeArrayCollection& objects) {}
  virtual void visitGenStmtCollection(const Any* object, const GenStmtCollection& objects) {}
  virtual void visitHierPathCollection(const Any* object, const HierPathCollection& objects) {}
  virtual void visitIODeclCollection(const Any* object, const IODeclCollection& objects) {}
  virtual void visitIdentifierCollection(const Any* object, const IdentifierCollection& objects) {}
  virtual void visitIfElseCollection(const Any* object, const IfElseCollection& objects) {}
  virtual void visitIfStmtCollection(const Any* object, const IfStmtCollection& objects) {}
  virtual void visitImmediateAssertCollection(const Any* object, const ImmediateAssertCollection& objects) {}
  virtual void visitImmediateAssumeCollection(const Any* object, const ImmediateAssumeCollection& objects) {}
  virtual void visitImmediateCoverCollection(const Any* object, const ImmediateCoverCollection& objects) {}
  virtual void visitImplementsCollection(const Any* object, const ImplementsCollection& objects) {}
  virtual void visitImplicationCollection(const Any* object, const ImplicationCollection& objects) {}
  virtual void visitImportTypespecCollection(const Any* object, const ImportTypespecCollection& objects) {}
  virtual void visitIncludeStmtCollection(const Any* object, const IncludeStmtCollection& objects) {}
  virtual void visitIndexedPartSelectCollection(const Any* object, const IndexedPartSelectCollection& objects) {}
  virtual void visitInitialCollection(const Any* object, const InitialCollection& objects) {}
  virtual void visitInstanceCollection(const Any* object, const InstanceCollection& objects) {}
  virtual void visitInstanceArrayCollection(const Any* object, const InstanceArrayCollection& objects) {}
  virtual void visitIntTypespecCollection(const Any* object, const IntTypespecCollection& objects) {}
  virtual void visitIntegerTypespecCollection(const Any* object, const IntegerTypespecCollection& objects) {}
  virtual void visitInterfaceCollection(const Any* object, const InterfaceCollection& objects) {}
  virtual void visitInterfaceArrayCollection(const Any* object, const InterfaceArrayCollection& objects) {}
  virtual void visitInterfaceTFDeclCollection(const Any* object, const InterfaceTFDeclCollection& objects) {}
  virtual void visitInterfaceTypespecCollection(const Any* object, const InterfaceTypespecCollection& objects) {}
  virtual void visitLetDeclCollection(const Any* object, const LetDeclCollection& objects) {}
  virtual void visitLetExprCollection(const Any* object, const LetExprCollection& objects) {}
  virtual void visitLibraryCollection(const Any* object, const LibraryCollection& objects) {}
  virtual void visitLogicTypespecCollection(const Any* object, const LogicTypespecCollection& objects) {}
  virtual void visitLongIntTypespecCollection(const Any* object, const LongIntTypespecCollection& objects) {}
  virtual void visitMethodFuncCallCollection(const Any* object, const MethodFuncCallCollection& objects) {}
  virtual void visitMethodTaskCallCollection(const Any* object, const MethodTaskCallCollection& objects) {}
  virtual void visitModPathCollection(const Any* object, const ModPathCollection& objects) {}
  virtual void visitModportCollection(const Any* object, const ModportCollection& objects) {}
  virtual void visitModuleCollection(const Any* object, const ModuleCollection& objects) {}
  virtual void visitModuleArrayCollection(const Any* object, const ModuleArrayCollection& objects) {}
  virtual void visitModuleTypespecCollection(const Any* object, const ModuleTypespecCollection& objects) {}
  virtual void visitMulticlockSequenceExprCollection(const Any* object, const MulticlockSequenceExprCollection& objects) {}
  virtual void visitNamedEventCollection(const Any* object, const NamedEventCollection& objects) {}
  virtual void visitNamedEventArrayCollection(const Any* object, const NamedEventArrayCollection& objects) {}
  virtual void visitNetCollection(const Any* object, const NetCollection& objects) {}
  virtual void visitNullStmtCollection(const Any* object, const NullStmtCollection& objects) {}
  virtual void visitOperationCollection(const Any* object, const OperationCollection& objects) {}
  virtual void visitOrderedWaitCollection(const Any* object, const OrderedWaitCollection& objects) {}
  virtual void visitPackageCollection(const Any* object, const PackageCollection& objects) {}
  virtual void visitPackageTypespecCollection(const Any* object, const PackageTypespecCollection& objects) {}
  virtual void visitParamAssignCollection(const Any* object, const ParamAssignCollection& objects) {}
  virtual void visitParameterCollection(const Any* object, const ParameterCollection& objects) {}
  virtual void visitPartSelectCollection(const Any* object, const PartSelectCollection& objects) {}
  virtual void visitPathTermCollection(const Any* object, const PathTermCollection& objects) {}
  virtual void visitPortCollection(const Any* object, const PortCollection& objects) {}
  virtual void visitPortBitCollection(const Any* object, const PortBitCollection& objects) {}
  virtual void visitPortsCollection(const Any* object, const PortsCollection& objects) {}
  virtual void visitPreprocMacroDefinitionCollection(const Any* object, const PreprocMacroDefinitionCollection& objects) {}
  virtual void visitPreprocMacroInstanceCollection(const Any* object, const PreprocMacroInstanceCollection& objects) {}
  virtual void visitPrimTermCollection(const Any* object, const PrimTermCollection& objects) {}
  virtual void visitPrimitiveCollection(const Any* object, const PrimitiveCollection& objects) {}
  virtual void visitPrimitiveArrayCollection(const Any* object, const PrimitiveArrayCollection& objects) {}
  virtual void visitProcessCollection(const Any* object, const ProcessCollection& objects) {}
  virtual void visitProgramCollection(const Any* object, const ProgramCollection& objects) {}
  virtual void visitProgramArrayCollection(const Any* object, const ProgramArrayCollection& objects) {}
  virtual void visitProgramTypespecCollection(const Any* object, const ProgramTypespecCollection& objects) {}
  virtual void visitPropFormalDeclCollection(const Any* object, const PropFormalDeclCollection& objects) {}
  virtual void visitPropertyDeclCollection(const Any* object, const PropertyDeclCollection& objects) {}
  virtual void visitPropertyInstCollection(const Any* object, const PropertyInstCollection& objects) {}
  virtual void visitPropertySpecCollection(const Any* object, const PropertySpecCollection& objects) {}
  virtual void visitPropertyTypespecCollection(const Any* object, const PropertyTypespecCollection& objects) {}
  virtual void visitRangeCollection(const Any* object, const RangeCollection& objects) {}
  virtual void visitRealTypespecCollection(const Any* object, const RealTypespecCollection& objects) {}
  virtual void visitRefInstanceCollection(const Any* object, const RefInstanceCollection& objects) {}
  virtual void visitRefObjCollection(const Any* object, const RefObjCollection& objects) {}
  virtual void visitRefTypespecCollection(const Any* object, const RefTypespecCollection& objects) {}
  virtual void visitRegCollection(const Any* object, const RegCollection& objects) {}
  virtual void visitRegArrayCollection(const Any* object, const RegArrayCollection& objects) {}
  virtual void visitReleaseCollection(const Any* object, const ReleaseCollection& objects) {}
  virtual void visitRepeatCollection(const Any* object, const RepeatCollection& objects) {}
  virtual void visitRepeatControlCollection(const Any* object, const RepeatControlCollection& objects) {}
  virtual void visitRestrictCollection(const Any* object, const RestrictCollection& objects) {}
  virtual void visitReturnStmtCollection(const Any* object, const ReturnStmtCollection& objects) {}
  virtual void visitScopeCollection(const Any* object, const ScopeCollection& objects) {}
  virtual void visitSelectCollection(const Any* object, const SelectCollection& objects) {}
  virtual void visitSeqFormalDeclCollection(const Any* object, const SeqFormalDeclCollection& objects) {}
  virtual void visitSequenceDeclCollection(const Any* object, const SequenceDeclCollection& objects) {}
  virtual void visitSequenceInstCollection(const Any* object, const SequenceInstCollection& objects) {}
  virtual void visitSequenceTypespecCollection(const Any* object, const SequenceTypespecCollection& objects) {}
  virtual void visitShortIntTypespecCollection(const Any* object, const ShortIntTypespecCollection& objects) {}
  virtual void visitShortRealTypespecCollection(const Any* object, const ShortRealTypespecCollection& objects) {}
  virtual void visitSimpleExprCollection(const Any* object, const SimpleExprCollection& objects) {}
  virtual void visitSoftDisableCollection(const Any* object, const SoftDisableCollection& objects) {}
  virtual void visitSourceFileCollection(const Any* object, const SourceFileCollection& objects) {}
  virtual void visitSpecParamCollection(const Any* object, const SpecParamCollection& objects) {}
  virtual void visitStringTypespecCollection(const Any* object, const StringTypespecCollection& objects) {}
  virtual void visitStructPatternCollection(const Any* object, const StructPatternCollection& objects) {}
  virtual void visitStructTypespecCollection(const Any* object, const StructTypespecCollection& objects) {}
  virtual void visitSwitchArrayCollection(const Any* object, const SwitchArrayCollection& objects) {}
  virtual void visitSwitchTranCollection(const Any* object, const SwitchTranCollection& objects) {}
  virtual void visitSysFuncCallCollection(const Any* object, const SysFuncCallCollection& objects) {}
  virtual void visitSysTaskCallCollection(const Any* object, const SysTaskCallCollection& objects) {}
  virtual void visitTFCallCollection(const Any* object, const TFCallCollection& objects) {}
  virtual void visitTableEntryCollection(const Any* object, const TableEntryCollection& objects) {}
  virtual void visitTaggedPatternCollection(const Any* object, const TaggedPatternCollection& objects) {}
  virtual void visitTaskCollection(const Any* object, const TaskCollection& objects) {}
  virtual void visitTaskCallCollection(const Any* object, const TaskCallCollection& objects) {}
  virtual void visitTaskDeclCollection(const Any* object, const TaskDeclCollection& objects) {}
  virtual void visitTaskFuncCollection(const Any* object, const TaskFuncCollection& objects) {}
  virtual void visitTaskFuncDeclCollection(const Any* object, const TaskFuncDeclCollection& objects) {}
  virtual void visitTchkCollection(const Any* object, const TchkCollection& objects) {}
  virtual void visitTchkTermCollection(const Any* object, const TchkTermCollection& objects) {}
  virtual void visitThreadCollection(const Any* object, const ThreadCollection& objects) {}
  virtual void visitTimeTypespecCollection(const Any* object, const TimeTypespecCollection& objects) {}
  virtual void visitTypeParameterCollection(const Any* object, const TypeParameterCollection& objects) {}
  virtual void visitTypedefTypespecCollection(const Any* object, const TypedefTypespecCollection& objects) {}
  virtual void visitTypespecCollection(const Any* object, const TypespecCollection& objects) {}
  virtual void visitTypespecMemberCollection(const Any* object, const TypespecMemberCollection& objects) {}
  virtual void visitUdpCollection(const Any* object, const UdpCollection& objects) {}
  virtual void visitUdpArrayCollection(const Any* object, const UdpArrayCollection& objects) {}
  virtual void visitUdpDefnCollection(const Any* object, const UdpDefnCollection& objects) {}
  virtual void visitUdpDefnTypespecCollection(const Any* object, const UdpDefnTypespecCollection& objects) {}
  virtual void visitUnionTypespecCollection(const Any* object, const UnionTypespecCollection& objects) {}
  virtual void visitUniquenessCollection(const Any* object, const UniquenessCollection& objects) {}
  virtual void visitUnsupportedExprCollection(const Any* object, const UnsupportedExprCollection& objects) {}
  virtual void visitUnsupportedStmtCollection(const Any* object, const UnsupportedStmtCollection& objects) {}
  virtual void visitUnsupportedTypespecCollection(const Any* object, const UnsupportedTypespecCollection& objects) {}
  virtual void visitUserSystfCollection(const Any* object, const UserSystfCollection& objects) {}
  virtual void visitVarSelectCollection(const Any* object, const VarSelectCollection& objects) {}
  virtual void visitVariableCollection(const Any* object, const VariableCollection& objects) {}
  virtual void visitVoidTypespecCollection(const Any* object, const VoidTypespecCollection& objects) {}
  virtual void visitWaitForkCollection(const Any* object, const WaitForkCollection& objects) {}
  virtual void visitWaitStmtCollection(const Any* object, const WaitStmtCollection& objects) {}
  virtual void visitWaitsCollection(const Any* object, const WaitsCollection& objects) {}
  virtual void visitWhileStmtCollection(const Any* object, const WhileStmtCollection& objects) {}
  // clang-format on

private:
  // clang-format off
  void visitAny_(const Any* object);
  void visitAlias_(const Alias* object);
  void visitAlways_(const Always* object);
  void visitAnyPattern_(const AnyPattern* object);
  void visitArrayExpr_(const ArrayExpr* object);
  void visitArrayTypespec_(const ArrayTypespec* object);
  void visitAssert_(const Assert* object);
  void visitAssignStmt_(const AssignStmt* object);
  void visitAssignment_(const Assignment* object);
  void visitAssume_(const Assume* object);
  void visitAtomicStmt_(const AtomicStmt* object);
  void visitAttribute_(const Attribute* object);
  void visitBegin_(const Begin* object);
  void visitBindDirective_(const BindDirective* object);
  void visitBitSelect_(const BitSelect* object);
  void visitBitTypespec_(const BitTypespec* object);
  void visitBreakStmt_(const BreakStmt* object);
  void visitByteTypespec_(const ByteTypespec* object);
  void visitCaseItem_(const CaseItem* object);
  void visitCasePropertyItem_(const CasePropertyItem* object);
  void visitCaseProperty_(const CaseProperty* object);
  void visitCaseStmt_(const CaseStmt* object);
  void visitChandleTypespec_(const ChandleTypespec* object);
  void visitCheckerDecl_(const CheckerDecl* object);
  void visitCheckerInstPort_(const CheckerInstPort* object);
  void visitCheckerInst_(const CheckerInst* object);
  void visitCheckerPort_(const CheckerPort* object);
  void visitClassDefn_(const ClassDefn* object);
  void visitClassObj_(const ClassObj* object);
  void visitClassTypespec_(const ClassTypespec* object);
  void visitClause_(const Clause* object);
  void visitClockedProperty_(const ClockedProperty* object);
  void visitClockedSeq_(const ClockedSeq* object);
  void visitClockingBlock_(const ClockingBlock* object);
  void visitClockingIODecl_(const ClockingIODecl* object);
  void visitComment_(const Comment* object);
  void visitConcurrentAssertions_(const ConcurrentAssertions* object);
  void visitConfigDecl_(const ConfigDecl* object);
  void visitConfigRule_(const ConfigRule* object);
  void visitConstant_(const Constant* object);
  void visitConstrForeach_(const ConstrForeach* object);
  void visitConstrIfElse_(const ConstrIfElse* object);
  void visitConstrIf_(const ConstrIf* object);
  void visitConstraintExpr_(const ConstraintExpr* object);
  void visitConstraintOrdering_(const ConstraintOrdering* object);
  void visitConstraint_(const Constraint* object);
  void visitContAssignBit_(const ContAssignBit* object);
  void visitContAssign_(const ContAssign* object);
  void visitContinueStmt_(const ContinueStmt* object);
  void visitCoverBin_(const CoverBin* object);
  void visitCoverCross_(const CoverCross* object);
  void visitCoverGroup_(const CoverGroup* object);
  void visitCoverPoint_(const CoverPoint* object);
  void visitCover_(const Cover* object);
  void visitCoverageOption_(const CoverageOption* object);
  void visitDeassign_(const Deassign* object);
  void visitDefParam_(const DefParam* object);
  void visitDelayControl_(const DelayControl* object);
  void visitDelayTerm_(const DelayTerm* object);
  void visitDesign_(const Design* object);
  void visitDisableFork_(const DisableFork* object);
  void visitDisable_(const Disable* object);
  void visitDisables_(const Disables* object);
  void visitDistItem_(const DistItem* object);
  void visitDistribution_(const Distribution* object);
  void visitDoWhile_(const DoWhile* object);
  void visitEnumConst_(const EnumConst* object);
  void visitEnumTypespec_(const EnumTypespec* object);
  void visitEventControl_(const EventControl* object);
  void visitEventStmt_(const EventStmt* object);
  void visitEventTypespec_(const EventTypespec* object);
  void visitExpectStmt_(const ExpectStmt* object);
  void visitExpr_(const Expr* object);
  void visitExtends_(const Extends* object);
  void visitFinalStmt_(const FinalStmt* object);
  void visitForStmt_(const ForStmt* object);
  void visitForce_(const Force* object);
  void visitForeachStmt_(const ForeachStmt* object);
  void visitForeverStmt_(const ForeverStmt* object);
  void visitForkStmt_(const ForkStmt* object);
  void visitFuncCall_(const FuncCall* object);
  void visitFunctionDecl_(const FunctionDecl* object);
  void visitFunction_(const Function* object);
  void visitGateArray_(const GateArray* object);
  void visitGate_(const Gate* object);
  void visitGenCase_(const GenCase* object);
  void visitGenFor_(const GenFor* object);
  void visitGenIfElse_(const GenIfElse* object);
  void visitGenIf_(const GenIf* object);
  void visitGenRegion_(const GenRegion* object);
  void visitGenScopeArray_(const GenScopeArray* object);
  void visitGenScope_(const GenScope* object);
  void visitGenStmt_(const GenStmt* object);
  void visitHierPath_(const HierPath* object);
  void visitIODecl_(const IODecl* object);
  void visitIdentifier_(const Identifier* object);
  void visitIfElse_(const IfElse* object);
  void visitIfStmt_(const IfStmt* object);
  void visitImmediateAssert_(const ImmediateAssert* object);
  void visitImmediateAssume_(const ImmediateAssume* object);
  void visitImmediateCover_(const ImmediateCover* object);
  void visitImplements_(const Implements* object);
  void visitImplication_(const Implication* object);
  void visitImportTypespec_(const ImportTypespec* object);
  void visitIncludeStmt_(const IncludeStmt* object);
  void visitIndexedPartSelect_(const IndexedPartSelect* object);
  void visitInitial_(const Initial* object);
  void visitInstanceArray_(const InstanceArray* object);
  void visitInstance_(const Instance* object);
  void visitIntTypespec_(const IntTypespec* object);
  void visitIntegerTypespec_(const IntegerTypespec* object);
  void visitInterfaceArray_(const InterfaceArray* object);
  void visitInterfaceTFDecl_(const InterfaceTFDecl* object);
  void visitInterfaceTypespec_(const InterfaceTypespec* object);
  void visitInterface_(const Interface* object);
  void visitLetDecl_(const LetDecl* object);
  void visitLetExpr_(const LetExpr* object);
  void visitLibrary_(const Library* object);
  void visitLogicTypespec_(const LogicTypespec* object);
  void visitLongIntTypespec_(const LongIntTypespec* object);
  void visitMethodFuncCall_(const MethodFuncCall* object);
  void visitMethodTaskCall_(const MethodTaskCall* object);
  void visitModPath_(const ModPath* object);
  void visitModport_(const Modport* object);
  void visitModuleArray_(const ModuleArray* object);
  void visitModuleTypespec_(const ModuleTypespec* object);
  void visitModule_(const Module* object);
  void visitMulticlockSequenceExpr_(const MulticlockSequenceExpr* object);
  void visitNamedEventArray_(const NamedEventArray* object);
  void visitNamedEvent_(const NamedEvent* object);
  void visitNet_(const Net* object);
  void visitNullStmt_(const NullStmt* object);
  void visitOperation_(const Operation* object);
  void visitOrderedWait_(const OrderedWait* object);
  void visitPackageTypespec_(const PackageTypespec* object);
  void visitPackage_(const Package* object);
  void visitParamAssign_(const ParamAssign* object);
  void visitParameter_(const Parameter* object);
  void visitPartSelect_(const PartSelect* object);
  void visitPathTerm_(const PathTerm* object);
  void visitPortBit_(const PortBit* object);
  void visitPort_(const Port* object);
  void visitPorts_(const Ports* object);
  void visitPreprocMacroDefinition_(const PreprocMacroDefinition* object);
  void visitPreprocMacroInstance_(const PreprocMacroInstance* object);
  void visitPrimTerm_(const PrimTerm* object);
  void visitPrimitiveArray_(const PrimitiveArray* object);
  void visitPrimitive_(const Primitive* object);
  void visitProcess_(const Process* object);
  void visitProgramArray_(const ProgramArray* object);
  void visitProgramTypespec_(const ProgramTypespec* object);
  void visitProgram_(const Program* object);
  void visitPropFormalDecl_(const PropFormalDecl* object);
  void visitPropertyDecl_(const PropertyDecl* object);
  void visitPropertyInst_(const PropertyInst* object);
  void visitPropertySpec_(const PropertySpec* object);
  void visitPropertyTypespec_(const PropertyTypespec* object);
  void visitRange_(const Range* object);
  void visitRealTypespec_(const RealTypespec* object);
  void visitRefInstance_(const RefInstance* object);
  void visitRefObj_(const RefObj* object);
  void visitRefTypespec_(const RefTypespec* object);
  void visitRegArray_(const RegArray* object);
  void visitReg_(const Reg* object);
  void visitRelease_(const Release* object);
  void visitRepeatControl_(const RepeatControl* object);
  void visitRepeat_(const Repeat* object);
  void visitRestrict_(const Restrict* object);
  void visitReturnStmt_(const ReturnStmt* object);
  void visitScope_(const Scope* object);
  void visitSelect_(const Select* object);
  void visitSeqFormalDecl_(const SeqFormalDecl* object);
  void visitSequenceDecl_(const SequenceDecl* object);
  void visitSequenceInst_(const SequenceInst* object);
  void visitSequenceTypespec_(const SequenceTypespec* object);
  void visitShortIntTypespec_(const ShortIntTypespec* object);
  void visitShortRealTypespec_(const ShortRealTypespec* object);
  void visitSimpleExpr_(const SimpleExpr* object);
  void visitSoftDisable_(const SoftDisable* object);
  void visitSourceFile_(const SourceFile* object);
  void visitSpecParam_(const SpecParam* object);
  void visitStringTypespec_(const StringTypespec* object);
  void visitStructPattern_(const StructPattern* object);
  void visitStructTypespec_(const StructTypespec* object);
  void visitSwitchArray_(const SwitchArray* object);
  void visitSwitchTran_(const SwitchTran* object);
  void visitSysFuncCall_(const SysFuncCall* object);
  void visitSysTaskCall_(const SysTaskCall* object);
  void visitTFCall_(const TFCall* object);
  void visitTableEntry_(const TableEntry* object);
  void visitTaggedPattern_(const TaggedPattern* object);
  void visitTaskCall_(const TaskCall* object);
  void visitTaskDecl_(const TaskDecl* object);
  void visitTaskFuncDecl_(const TaskFuncDecl* object);
  void visitTaskFunc_(const TaskFunc* object);
  void visitTask_(const Task* object);
  void visitTchkTerm_(const TchkTerm* object);
  void visitTchk_(const Tchk* object);
  void visitThread_(const Thread* object);
  void visitTimeTypespec_(const TimeTypespec* object);
  void visitTypeParameter_(const TypeParameter* object);
  void visitTypedefTypespec_(const TypedefTypespec* object);
  void visitTypespecMember_(const TypespecMember* object);
  void visitTypespec_(const Typespec* object);
  void visitUdpArray_(const UdpArray* object);
  void visitUdpDefnTypespec_(const UdpDefnTypespec* object);
  void visitUdpDefn_(const UdpDefn* object);
  void visitUdp_(const Udp* object);
  void visitUnionTypespec_(const UnionTypespec* object);
  void visitUniqueness_(const Uniqueness* object);
  void visitUnsupportedExpr_(const UnsupportedExpr* object);
  void visitUnsupportedStmt_(const UnsupportedStmt* object);
  void visitUnsupportedTypespec_(const UnsupportedTypespec* object);
  void visitUserSystf_(const UserSystf* object);
  void visitVarSelect_(const VarSelect* object);
  void visitVariable_(const Variable* object);
  void visitVoidTypespec_(const VoidTypespec* object);
  void visitWaitFork_(const WaitFork* object);
  void visitWaitStmt_(const WaitStmt* object);
  void visitWaits_(const Waits* object);
  void visitWhileStmt_(const WhileStmt* object);
  // clang-format on

protected:
  AnySet m_visited;
  any_stack_t m_callstack;
  bool m_abortRequested = false;
};
}  // namespace uhdm

#endif  // UHDM_UHDMVISITOR_H
