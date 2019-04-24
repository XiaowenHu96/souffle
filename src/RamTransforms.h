/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamTransforms.h
 *
 * Defines RAM transformation passes.
 *
 ***********************************************************************/

#pragma once

#include "RamConditionLevel.h"
#include "RamConstValue.h"
#include "RamExpressionLevel.h"
#include "RamTransformer.h"
#include "RamTranslationUnit.h"
#include <memory>
#include <string>

namespace souffle {

class RamProgram;

/**
 * Hoists the conditions to the earliest point in the loop nest where they
 * can be evaluated.
 *
 * hoistConditions assumes that filter operations are stored verbose,
 * i.e. a conjunction is expressed by two consecutive filter operations.
 * For example ..
 *
 *  QUERY
 *   ...
 *    IF C1 /\ C2 then
 *     ...
 *
 * should be rewritten / or produced by the translator as
 *
 *  QUERY
 *   ...
 *    IF C1
 *     IF C2
 *      ...
 *
 * otherwise the levelling becomes imprecise, i.e., for both conditions
 * the most outer-level is sought rather than considered separately.
 *
 * If there are transformers prior to hoistConditions() that introduce
 * conjunction, another transformer is required that splits the
 * filter operations. However, at the moment this is not necessary
 * because the translator delivers already the right RAM format.
 */
class HoistConditionsTransformer : public RamTransformer {
public:
    std::string getName() const override {
        return "HoistConditionsTransformer";
    }

    /** hoist all conditions in a RAM program to the outermost scope */
    bool hoistConditions(RamProgram& program);

protected:
    RamConditionLevelAnalysis* rcla{nullptr};

    bool transform(RamTranslationUnit& translationUnit) override {
        rcla = translationUnit.getAnalysis<RamConditionLevelAnalysis>();
        return hoistConditions(*translationUnit.getProgram());
    }
};

/**
 * Make indexable operations to indexed operations.
 *
 * makeIndex() assumes that the RAM has been levelled before, and
 * that the conditions that could be used for an index are located
 * immediately after the scan or aggregrate operation
 *
 *  QUERY
 *   ...
 *   FOR t1 in A
 *    IF t1.x = 10 /\ t1.y = 20 /\ C
 *     ...
 *
 * will be rewritten to
 *
 *  QUERY
 *   ...
 *    SEARCH t1 in A INDEX t1.x=10 and t1.y = 20
 *     IF C
 *      ...
 */

class MakeIndexTransformer : public RamTransformer {
public:
    std::string getName() const override {
        return "MakeIndexTransformer";
    }

    /** Get expression of an equivalence relation of the format t1.x = <expr> or <expr> = t1.x */
    std::unique_ptr<RamExpression> getExpression(RamCondition* c, size_t& element, int level);

    /** Construct patterns for an indexable operation and the remaining condition that cannot be indexed */
    std::unique_ptr<RamCondition> constructPattern(std::vector<std::unique_ptr<RamExpression>>& queryPattern,
            bool& indexable, std::vector<std::unique_ptr<RamCondition>> conditionList, int identifier);

    /** Rewrite a scan operation to an indexed scan operation */
    std::unique_ptr<RamOperation> rewriteScan(const RamScan* scan);

    /** Rewrite an aggregate operation to an indexed aggregate operation */
    std::unique_ptr<RamOperation> rewriteAggregate(const RamAggregate* agg);

    /** Make indexable RAM operation indexed */
    bool makeIndex(RamProgram& program);

protected:
    RamExpressionLevelAnalysis* rvla{nullptr};
    RamConstValueAnalysis* rcva{nullptr};
    bool transform(RamTranslationUnit& translationUnit) override {
        rvla = translationUnit.getAnalysis<RamExpressionLevelAnalysis>();
        rcva = translationUnit.getAnalysis<RamConstValueAnalysis>();
        return makeIndex(*translationUnit.getProgram());
    }
};

/**
 * Convert IndexScan operations to Filter/Existence Checks
 * if the IndexScan's tuple is not further used in subsequent
 * operations.
 *
 *  QUERY
 *   ...
 *    SEARCH t1 IN A INDEX t1.x=10 AND t1.y = 20
 *      ... // no occurrence of t1
 *
 * will be rewritten to
 *
 *  QUERY
 *   ...
 *    IF (10,20) NOT IN A
 *      ...
 *
 */

class IfConversionTransformer : public RamTransformer {
public:
    std::string getName() const override {
        return "IfConversionTransformer";
    }
    std::unique_ptr<RamOperation> rewriteIndexScan(const RamIndexScan* indexScan);
    bool convertExistenceChecks(RamProgram& program);

protected:
    bool transform(RamTranslationUnit& translationUnit) override {
        return convertExistenceChecks(*translationUnit.getProgram());
    }
};

}  // end of namespace souffle
