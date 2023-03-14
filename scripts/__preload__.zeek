
# doc-common-start
module Spicy;

export {
# doc-options-start
    ## Constant for testing if Spicy is available.
    const available = T;

    ## Show output of Spicy print statements.
    const enable_print = F &redef;

    # Record and display profiling information, if compiled into analyzer.
    const enable_profiling = F &redef;

    ## abort() instead of throwing HILTI exceptions.
    const abort_on_exceptions = F &redef;

    ## Include backtraces when reporting unhandled exceptions.
    const show_backtraces = F &redef;

    ## Maximum depth of recursive file analysis (Spicy analyzers only)
    const max_file_depth: count = 5 &redef;
# doc-options-end

# doc-types-start
    ## Result type for `Spicy::resource_usage()`.
    type ResourceUsage: record {
        user_time : interval;           ##< User CPU time of the Zeek process.
        system_time :interval;          ##< System CPU time of the Zeek process.
        memory_heap : count;            ##< Memory allocated on the heap by the Zeek process
        num_fibers : count;             ##< Number of fibers currently in use.
        max_fibers: count;              ##< Maximum number of fibers ever in use.
        max_fiber_stack_size: count;    ##< Maximum fiber stack size ever in use.
        cached_fibers: count;           ##< Number of fibers currently cached.
    };
# doc-types-end
}
