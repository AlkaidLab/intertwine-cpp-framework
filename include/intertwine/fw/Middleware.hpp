#ifndef INTERTWINE_CPP_FRAMEWORK_MIDDLEWARE_HPP
#define INTERTWINE_CPP_FRAMEWORK_MIDDLEWARE_HPP

/**
 * Onion-model middleware chain.
 *
 * Each middleware receives (Context&, Next). Calling next() enters the inner
 * middleware + handler. Code before next() is "before" logic;
 * code after is "after" logic.
 *
 * Return: 0 = continue/success, non-zero = abort with that status code
 *         (compatible with libhv convention where >0 triggers SendHttpResponse).
 * If a middleware does NOT call next(), the chain stops early (e.g. auth failure).
 */

#include <functional>
#include <vector>
#include <string>

namespace intertwine {
namespace fw {

class Context;

using Next = std::function<int()>;
using MiddlewareFn = std::function<int(Context&, Next)>;

/**
 * Ordered middleware chain with onion-model execution.
 */
class MiddlewareChain {
public:
    /** Append a middleware */
    void use(MiddlewareFn fn);

    /** Append a named middleware (for debugging / removal) */
    void use(const std::string& name, MiddlewareFn fn);

    /** Execute the full chain; innermost layer calls finalHandler */
    int execute(Context& ctx, std::function<int(Context&)> finalHandler);

    size_t size() const { return m_chain.size(); }
    void clear() { m_chain.clear(); }

private:
    struct Entry {
        std::string name;
        MiddlewareFn fn;
    };
    std::vector<Entry> m_chain;

    int run(size_t idx, Context& ctx, std::function<int(Context&)>& finalHandler);
};

} // namespace fw
} // namespace intertwine

#endif // INTERTWINE_CPP_FRAMEWORK_MIDDLEWARE_HPP
