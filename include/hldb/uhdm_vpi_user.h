/*

 Copyright 2019-2021 Alain Dargelas

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
#ifndef UHDM_VPI_USER_H
#define UHDM_VPI_USER_H
#pragma once

#include <uhdm/sv_vpi_user.h>

// Missing defines from vpi_user.h, sv_vpi_user.h
#define vpiDesign                     3000
#define vpiInterfaceTypespec          3001
#define vpiNets                       3002
#define vpiSimpleExpr                 3003
#define vpiParameters                 3004
#define vpiSequenceExpr               3005
#define vpiSoftDisable                3006
#define vpiIsModPort                  3007
#define vpiValue                      3008
#define vpiClocking                   3009
#define vpiImplements                 3010
#define vpiPathElem                   3011 // Path elem of a hierarchical path or typespec

// These define where orinally aliased in sv_vpi_user.h
// Aliasing makes it hard to distinguish in automatically generated code, assigning unique values.
#define vpiVarBit                     3101
#define vpiLogicVar                   3102
#define vpiArrayVar                   3103

#define vpiWaits                      3104
#define vpiDisables                   3105
#define vpiStructMember               3106

// Used to mark imported parameters
#define vpiImported                   3150
#define vpiImportExport               3151
#define vpiExported                   3152
#define vpiPreprocFile                3153

// Extra location information
#define vpiStartLine                  vpiLineNo
#define vpiStartColumn                3200
#define vpiEndLine                    3201
#define vpiEndColumn                  3202

#define vpiAllPackages                3210
#define vpiTopPackages                3211
#define vpiAllClasses                 3212
#define vpiAllInterfaces              3213
#define vpiAllUdps                    3214
#define vpiAllPrograms                3215
#define vpiAllModules                 3216
#define vpiTopModules                 3217
// Include and source file details
#define vpiSourceFile                 3218
#define vpiIncludes                   3219

// Preprocessor directives
#define vpiPreprocMacroDefinition     3250
#define vpiPreprocMacroInstance       3251
#define vpiNameStartColumn            3252
#define vpiBodyStartColumn            3253
#define vpiToken                      3254
#define vpiObject                     3255
#define vpiSectionStartLine           3256
#define vpiSectionStartColumn         3257
#define vpiSectionEndLine             3258
#define vpiSectionEndColumn           3259
#define vpiSourceStartLine            3260
#define vpiSourceStartColumn          3261
#define vpiSourceEndLine              3262
#define vpiSourceEndColumn            3263

// Tags used to model unsupported nodes
#define vpiUnsupportedStmt            3301
#define vpiUnsupportedExpr            3302
#define vpiUnsupportedTypespec        3303

// Objects/properties not in the Standard
#define vpiHierPath                   3500 // Represents a hierarchical path
#define vpiReordered                  3501 // Boolean for operations (pattern assign, concat) that has been reordered
#define vpiElaborated                 3502 // Boolean indicating UHDM has been elaborated/uniquified
#define vpiRefVar                     3503 // "variables" type reference object required for late binding during elaboration
#define vpiOverriden                  3504 // Boolean indicating a param_assign is overriden (not default value)
#define vpiFlattened                  3505 // Boolean indicating an operation (pattern assign) has already been flattened
#define vpiCheckerDecl                3506 // Handle to checker_def
#define vpiCheckerInst                3507 // Handle to checker_inst
#define vpiCheckerPort                3508 // Handle to checker_port
#define vpiCheckerInstPort            3509 // Handle to checker_inst_port
#define vpiArrayExpr                  3510 // Handle to array_expr
#define vpiRefInstance                3511 // Handle to instance ref for folded (Non-elaborated) model
#define vpiGenStmt                    3512 // Handle to generate stmt for folded (Non-elaborated) model
#define vpiGenIf                      3513 // Handle to if-generate for folded (Non-elaborated) model
#define vpiGenIfElse                  3514 // Handle to if-else-generate for folded (Non-elaborated) model
#define vpiGenFor                     3515 // Handle to for-generate for folded (Non-elaborated) model
#define vpiGenCase                    3516 // Handle to case-generate for folded (Non-elaborated) model
#define vpiGenRegion                  3517 // Handle to generate region for folded (Non-elaborated) model
#define vpiIdentifier                 3518 // Hold string identifiers (like name)
#define vpiSelect                     3519 // Abstract base for all selects (bit-select, part-select, etc)

// bind directive model constants
#define vpiBindDirective              3520 // Handle to bind_directive (IEEE 1800-2017 Section 23.11)
#define vpiBindTargetScope            3521 // Reference to the target module/interface scope identifier in a bind directive
#define vpiBindTargetInstance         3522 // Reference to the target instance identifier(s) in a bind directive
#define vpiBindSourceInstance         3523 // Reference to the bind_instantiation (ref_module or checker_inst) in a bind directive

// Library/Config model constants
#define vpiConfigRule                 3524 // config -> config_rule (IEEE 1800-2023 33.4.1)
#define vpiTopCell                    3525 // config_decl -> top cell name(s) (IEEE 1800-2023 33.4.1.1)
#define vpiConfigSelector             3526 // config_rule -> selector clause (default/instance/cell)
#define vpiConfigTarget               3527 // config_rule -> target clause (liblist/use)
#define vpiLiblist                    3528 // clause -> library name(s) (liblist kind)
#define vpiUseConfig                  3529 // Boolean: clause has the ':config' suffix (use kind, use_clause_config)
#define vpiClause                     3530 // Type tag for clause (config_rule selector/target; IEEE 1800-2023 33.4.1)
#define vpiClauseType                 3531 // clause subtypes (IEEE 1800-2023 33.4.1.2-33.4.1.6):
#define vpiDefaultClause                 1 // 'default' selector
#define vpiInstClause                    2 // 'instance' selector
#define vpiCellClause                    3 // 'cell' selector
#define vpiLiblistClause                 4 // 'liblist' target
#define vpiUseClause                     5 // 'use' target
#define vpiLibraryCell                3532 // library -> cells (module/udp/interface/program/package/config it holds)
#define vpiIncludeStmt                3533 // design -> include_statement AND include_statement type tag (IEEE 1800-2023 33.3.1)

// IEEE 1800-2017 Section 19 — Covergroup object types and relations
#define vpiCoverageOption             3534 // Handle to coverage_option (option./type_option.)
#define vpiCoverBin                   3535 // Handle to cover_bin
#define vpiBinsSelection              3536 // Handle to bins_selection
#define vpiCoverPoint                 3537 // Handle to cover_point
#define vpiCoverCross                 3538 // Handle to cover_cross
#define vpiCoverGroup                 3539 // Handle to cover_group

// Covergroup field/relation constants
#define vpiCoverageEvent              3540 // CoverGroup → coverage sampling event
#define vpiCrossItem                  3541 // CoverCross → cross_item cover_points (card:any)
#define vpiBinsArraySize              3542 // CoverBin → array size expression

// CoverGroup model constants
#define vpiBinsType                   3543 // int32_t: 0=bins, 1=illegal_bins, 2=ignore_bins
#define vpiBinsTypeBins                  1
#define vpiBinsTypeIllegal               2
#define vpiBinsTypeIgnore                3
#define vpiBinsIsWildcard             3544 // bool: wildcard bins
#define vpiBinsIsDefault              3545 // bool: default bins
#define vpiBinsIsDefaultSeq           3546 // bool: default sequence bins
#define vpiCoverageOptionType         3547 // int: type_option. vs option.
#define vpiCoverageOptionOption          1
#define vpiCoverageOptionTypeOption      2

// Checker model constants
#define vpiCheckerDecl                3548
#define vpiDefaultDisableIff          3549
#define vpiFormalType                 3550
// formal_type values for checker port items (vpiCheckerPort)
#define vpiFormalTypeData                1 // data_type_or_implicit
#define vpiFormalTypeSequence            2 // SEQUENCE keyword
#define vpiFormalTypeProperty            3 // PROPERTY keyword
#define vpiFormalTypeUntyped             4 // UNTYPED keyword

#endif  // UHDM_VPI_USER_H
