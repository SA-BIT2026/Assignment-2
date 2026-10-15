/**
 * ICFG.cpp
 * @author kisslune 
 */

#include "CFGA.h"

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

    // TODO: complete the following method: 'CFGAnalysis::analyze'
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
            // TODO: DFS the graph, starting from src and detecting the paths ending at snk.
            // Use the class method 'recordPath' (already defined) to record the path you detected.
            //@{
            std::set<std::pair<unsigned, std::vector<unsigned>>> visited;
            std::vector<unsigned> path;
            std::vector<unsigned> callStack;

            auto dfs = [&](auto &&self, unsigned cur) -> void
            {
                std::pair<unsigned, std::vector<unsigned>> key(cur, callStack);
                if (visited.find(key) != visited.end())
                    return;
                visited.insert(key);

                path.push_back(cur);

                if (cur == snk)
                    recordPath(path);

                for (ICFGEdge *edge : icfg->getICFGNode(cur)->getOutEdges())
                {
                    unsigned suc = edge->getDstNode()->getId();

                    if (CallCFGEdge *callEdge = SVFUtil::dyn_cast<CallCFGEdge>(edge))
                    {
                        callStack.push_back(callEdge->getCallSite()->getId());
                        self(self, suc);
                        callStack.pop_back();
                    }
                    else if (RetCFGEdge *retEdge = SVFUtil::dyn_cast<RetCFGEdge>(edge))
                    {
                        unsigned callsite = retEdge->getCallSite()->getId();
                        if (callStack.empty())
                        {
                            self(self, suc);
                        }
                        else if (callStack.back() == callsite)
                        {
                            callStack.pop_back();
                            self(self, suc);
                            callStack.push_back(callsite);
                        }
                    }
                    else
                    {
                        self(self, suc);
                    }
                }

                path.pop_back();
                visited.erase(key);
            };

            dfs(dfs, src);
            //@}
        }
}
