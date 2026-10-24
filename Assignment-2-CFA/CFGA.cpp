/**
 * ICFG.cpp
 * @author kisslune
 */

#include "CFGA.h"
#include <functional>

using namespace SVF;
using namespace llvm;
using namespace std;

int main(int argc, char **argv)
{
    auto moduleNameVec =
            OptionBase::parseOptions(argc, argv, "Whole Program Points-to Analysis",
                                     "[options] <input-bitcode...>");

    LLVMModuleSet::buildSVFModule(moduleNameVec);

    SVFIRBuilder builder;
    auto pag = builder.build();
    auto icfg = pag->getICFG();

    CFGAnalysis analyzer = CFGAnalysis(icfg);

    analyzer.analyze(icfg);

    analyzer.dumpPaths();
    LLVMModuleSet::releaseLLVMModuleSet();
    return 0;
}


void CFGAnalysis::analyze(SVF::ICFG *icfg)
{
    // Sources and sinks are specified when an analyzer is instantiated.
    for (auto src : sources)
        for (auto snk : sinks)
        {
            // Visited nodes are tracked per function activation: taking a call edge
            // opens a fresh scope, so the very same function can be walked again at a
            // different call site (e.g. two calls to the same callee), while a loop
            // inside one activation is still cut off.
            vector<set<unsigned>> scopes;
            scopes.emplace_back();
            scopes.back().insert(src);

            vector<unsigned> path;

            // DFS the graph, starting from src and detecting the paths ending at snk.
            // Use the class method 'recordPath' (already defined) to record the path you detected.
            function<void(unsigned)> dfs = [&](unsigned cur)
            {
                path.push_back(cur);

                if (cur == snk)
                {
                    recordPath(path);
                    path.pop_back();
                    return;
                }

                for (auto edge : icfg->getICFGNode(cur)->getOutEdges())
                {
                    ICFGNode *dst = edge->getDstNode();
                    unsigned nxt = dst->getId();

                    if (edge->isCallCFGEdge())
                    {
                        // The callee is already being walked -> recursion, stop here.
                        bool active = false;
                        for (const auto &scope : scopes)
                            if (scope.count(nxt))
                            {
                                active = true;
                                break;
                            }
                        if (active)
                            continue;

                        // Remember the call site, so that only its own return site is
                        // taken once the callee has finished.
                        callStack.push(cur);
                        scopes.emplace_back();
                        scopes.back().insert(nxt);
                        dfs(nxt);
                        scopes.pop_back();
                        callStack.pop();
                    }
                    else if (edge->isRetCFGEdge())
                    {
                        if (callStack.empty())
                            continue;
                        const RetICFGNode *ret = SVFUtil::dyn_cast<RetICFGNode>(dst);
                        if (ret == nullptr || ret->getCallICFGNode() == nullptr)
                            continue;
                        // A callee exit fans out to the return site of every call site,
                        // so only the one matching the pending call is a real return.
                        if (ret->getCallICFGNode()->getId() != callStack.top())
                            continue;

                        unsigned pending = callStack.top();
                        set<unsigned> callee = scopes.back();
                        scopes.pop_back();
                        callStack.pop();

                        if (scopes.back().insert(nxt).second)
                        {
                            dfs(nxt);
                            scopes.back().erase(nxt);
                        }

                        scopes.push_back(callee);
                        callStack.push(pending);
                    }
                    else
                    {
                        if (scopes.back().insert(nxt).second)
                        {
                            dfs(nxt);
                            scopes.back().erase(nxt);
                        }
                    }
                }

                path.pop_back();
            };

            dfs(src);
        }
}
