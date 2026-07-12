#include "intertwine/fw/Middleware.hpp"
#include "intertwine/fw/Context.hpp"
#include <utility>  // std::move

namespace intertwine {
namespace fw {

void MiddlewareChain::use(MiddlewareFn fn) {
    Entry e;
    e.fn = std::move(fn);
    m_chain.push_back(std::move(e));
}

void MiddlewareChain::use(const std::string& name, MiddlewareFn fn) {
    Entry e;
    e.name = name;
    e.fn = std::move(fn);
    m_chain.push_back(std::move(e));
}

int MiddlewareChain::execute(Context& ctx, std::function<int(Context&)> finalHandler) {
    return run(0, ctx, finalHandler);
}

int MiddlewareChain::run(size_t idx, Context& ctx,
                         std::function<int(Context&)>& finalHandler) {
    if (idx >= m_chain.size()) {
        /* All middleware executed, call innermost handler */
        return finalHandler(ctx);
    }
    /* Capture idx+1 to build next closure */
    return m_chain[idx].fn(ctx, [this, idx, &ctx, &finalHandler]() -> int {
        return run(idx + 1, ctx, finalHandler);
    });
}

} // namespace fw
} // namespace intertwine
