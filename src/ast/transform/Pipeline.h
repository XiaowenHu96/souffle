/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Pipeline.h
 *
 * Transformer that holds an arbitrary number of sub-transformations
 *
 ***********************************************************************/

#pragma once

#include "ast/TranslationUnit.h"
#include "ast/transform/DebugReporter.h"
#include "ast/transform/Meta.h"
#include "ast/transform/Null.h"
#include "ast/transform/Transformer.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

/**
 * Transformer that holds an arbitrary number of sub-transformations
 */
class PipelineTransformer : public MetaTransformer {
public:
    template <typename... Args>
    PipelineTransformer(Args... args) {
        Own<AstTransformer> tmp[] = {std::move(args)...};
        for (auto& cur : tmp) {
            pipeline.push_back(std::move(cur));
        }
    }

    PipelineTransformer(VecOwn<AstTransformer> pipeline) : pipeline(std::move(pipeline)) {}

    std::vector<AstTransformer*> getSubtransformers() const override {
        return toPtrVector(pipeline);
    }

    void setDebugReport() override {
        for (auto& i : pipeline) {
            if (auto* mt = dynamic_cast<MetaTransformer*>(i.get())) {
                mt->setDebugReport();
            } else {
                i = std::make_unique<DebugReporter>(std::move(i));
            }
        }
    }

    void setVerbosity(bool verbose) override {
        this->verbose = verbose;
        for (auto& cur : pipeline) {
            if (auto* mt = dynamic_cast<MetaTransformer*>(cur.get())) {
                mt->setVerbosity(verbose);
            }
        }
    }

    void disableTransformers(const std::set<std::string>& transforms) override {
        for (auto& i : pipeline) {
            if (auto* mt = dynamic_cast<MetaTransformer*>(i.get())) {
                mt->disableTransformers(transforms);
            } else if (transforms.find(i->getName()) != transforms.end()) {
                i = std::make_unique<NullTransformer>();
            }
        }
    }

    std::string getName() const override {
        return "PipelineTransformer";
    }

    PipelineTransformer* clone() const override {
        VecOwn<AstTransformer> transformers;
        for (const auto& transformer : pipeline) {
            transformers.push_back(souffle::clone(transformer));
        }
        return new PipelineTransformer(std::move(transformers));
    }

protected:
    VecOwn<AstTransformer> pipeline;
    bool transform(AstTranslationUnit& translationUnit) override {
        bool changed = false;
        for (auto& transformer : pipeline) {
            changed |= applySubtransformer(translationUnit, transformer.get());
        }
        return changed;
    }
};

}  // end of namespace souffle
