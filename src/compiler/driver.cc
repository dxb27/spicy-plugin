// Copyright (c) 2020-2021 by the Zeek Project. See LICENSE for details.

#include <getopt.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <hilti/ast/declarations/type.h>

#include <spicy/ast/detail/visitor.h>
#include <spicy/ast/types/unit.h>
#include <spicy/autogen/config.h>
#include <zeek-spicy/autogen/config.h>
#include <zeek-spicy/compiler/driver.h>
#include <zeek-spicy/compiler/glue-compiler.h>
#include <zeek-spicy/compiler/debug.h>

// Must come after Bro includes to avoid namespace conflicts.
#include <spicy/rt/libspicy.h>

#if SPICY_VERSION_NUMBER >= 10500
#include <hilti/compiler/init.h>

#include <spicy/compiler/init.h>
#endif

using namespace spicy::zeek;
using Driver = spicy::zeek::Driver;

/** Visitor to extract unit information from an HILTI AST before it's compiled. */
struct VisitorPreCompilation : public hilti::visitor::PreOrder<void, VisitorPreCompilation> {
    explicit VisitorPreCompilation(Driver* driver, hilti::ID module, hilti::rt::filesystem::path path)
        : driver(driver), module(std::move(module)), path(std::move(path)) {}

    void operator()(const hilti::declaration::Type& t) {
        if ( auto et = t.type().tryAs<hilti::type::Enum>(); et && t.linkage() == hilti::declaration::Linkage::Public ) {
            auto ei = EnumInfo{
                .id = hilti::ID(module, t.id()),
                .type = *et,
                .module_id = module,
                .module_path = path,
            };

            enums.emplace_back(std::move(ei));
        }
    }

    Driver* driver;
    hilti::ID module;
    hilti::rt::filesystem::path path;
    std::vector<EnumInfo> enums;
};

/** Visitor to extract unit information from an HILTI AST after it's compiled. */
struct VisitorPostCompilation : public hilti::visitor::PreOrder<void, VisitorPostCompilation> {
    explicit VisitorPostCompilation(Driver* driver, hilti::ID module, hilti::rt::filesystem::path path)
        : driver(driver), module(std::move(module)), path(std::move(path)) {}

    void operator()(const hilti::declaration::Type& t) {
        if ( auto ut = t.type().tryAs<spicy::type::Unit>() ) {
            auto ui = UnitInfo{
                .id = *t.type().typeID(),
                .type = *ut,
                .module_id = module,
                .module_path = path,
            };

            units.emplace_back(std::move(ui));
        }
    }

    Driver* driver;
    hilti::ID module;
    hilti::rt::filesystem::path path;
    std::vector<UnitInfo> units;
};

Driver::Driver(std::unique_ptr<GlueCompiler> glue, const char* argv0, hilti::rt::filesystem::path plugin_path,
               int zeek_version)
    : spicy::Driver("<Spicy Plugin for Zeek>"), _glue(std::move(glue)) {
    _glue->Init(this, zeek_version);

    spicy::Configuration::extendHiltiConfiguration();
    auto options = hiltiOptions();

    // Note that, different from Spicy's own SPICY_PATH, this extends the
    // search path, it doesn't replace it.
    if ( auto path = hilti::rt::getenv("ZEEK_SPICY_PATH") ) {
        for ( const auto& dir : hilti::rt::split(*path, ":") ) {
            if ( dir.size() )
                options.library_paths.emplace_back(dir);
        }
    }

    try {
        plugin_path = hilti::rt::filesystem::canonical(plugin_path);

        // We make our search paths relative to the plugin library, so that the
        // plugin instalation can move around.
        options.cxx_include_paths.push_back(plugin_path / "include");
        options.library_paths.push_back(plugin_path / "spicy");
    } catch ( const hilti::rt::filesystem::filesystem_error& e ) {
        ::hilti::logger().warning(
            hilti::util::fmt("invalid plugin base directory %s: %s", plugin_path.native(), e.what()));
    }

    for ( const auto& i : hilti::util::split(spicy::zeek::configuration::CxxZeekIncludeDirectories, ":") ) {
        if ( i.size() )
            options.cxx_include_paths.emplace_back(i);
    }

    if ( strlen(spicy::zeek::configuration::CxxBrokerIncludeDirectory) )
        options.cxx_include_paths.emplace_back(spicy::zeek::configuration::CxxBrokerIncludeDirectory);

#ifdef DEBUG
    ZEEK_DEBUG("Search paths:");

    auto hilti_options = hiltiOptions();
    for ( const auto& x : hilti_options.library_paths ) {
        ZEEK_DEBUG(hilti::rt::fmt("  %s", x.native()));
    }
#endif

    setCompilerOptions(std::move(options));

    auto& config = spicy::configuration();
    config.preprocessor_constants["HAVE_ZEEK"] = 1;
    config.preprocessor_constants["ZEEK_VERSION"] = zeek_version;

#if SPICY_VERSION_NUMBER >= 10500
    hilti::init();
    spicy::init();
#endif
}

Driver::~Driver() {}

hilti::Result<hilti::Nothing> Driver::loadFile(hilti::rt::filesystem::path file,
                                               const hilti::rt::filesystem::path& relative_to) {
    if ( ! relative_to.empty() && file.is_relative() ) {
        if ( auto p = relative_to / file; hilti::rt::filesystem::exists(p) )
            file = p;
    }

    if ( ! hilti::rt::filesystem::exists(file) ) {
        if ( auto path = hilti::util::findInPaths(file, hiltiOptions().library_paths) )
            file = *path;
        else
            return hilti::result::Error(hilti::util::fmt("Spicy plugin cannot find file %s", file));
    }

    auto rpath = hilti::util::normalizePath(file);
    auto ext = rpath.extension();

    if ( ext == ".evt" ) {
        ZEEK_DEBUG(hilti::util::fmt("Loading EVT file %s", rpath));
        if ( _glue->loadEvtFile(rpath) )
            return hilti::Nothing();
        else
            return hilti::result::Error(hilti::util::fmt("error loading EVT file %s", rpath));
    }

    if ( ext == ".spicy" ) {
        ZEEK_DEBUG(hilti::util::fmt("Loading Spicy file %s", rpath));
        if ( auto rc = addInput(rpath); ! rc )
            return rc.error();

        return hilti::Nothing();
    }

    if ( ext == ".hlt" ) {
        ZEEK_DEBUG(hilti::util::fmt("Loading HILTI file %s", rpath));
        if ( auto rc = addInput(rpath) )
            return hilti::Nothing();
        else
            return rc.error();
    }

    if ( ext == ".hlto" ) {
        ZEEK_DEBUG(hilti::util::fmt("Loading precompiled HILTI code %s", rpath));
        if ( auto rc = addInput(rpath) )
            return hilti::Nothing();
        else
            return rc.error();
    }

    if ( ext == ".cc" || ext == ".cxx" ) {
        ZEEK_DEBUG(hilti::util::fmt("Loading C++ code %s", rpath));
        if ( auto rc = addInput(rpath) )
            return hilti::Nothing();
        else
            return rc.error();
    }

    return hilti::result::Error(hilti::util::fmt("unknown file type passed to Spicy loader: %s", rpath));
}

hilti::Result<hilti::Nothing> Driver::compile() {
    if ( ! hasInputs() )
        return hilti::Nothing();

    ZEEK_DEBUG("Running Spicy driver");

    if ( auto x = spicy::Driver::compile(); ! x )
        return x.error();

    ZEEK_DEBUG("Done with Spicy driver");
    return hilti::Nothing();
}

hilti::Result<UnitInfo> Driver::lookupUnit(const hilti::ID& unit) {
    if ( auto x = _units.find(unit); x != _units.end() )
        return x->second;
    else
        return hilti::result::Error("unknown unit");
}

void Driver::hookNewASTPreCompilation(std::shared_ptr<hilti::Unit> unit) {
    if ( unit->extension() != ".spicy" )
        return;

    if ( unit->path().empty() )
        // Ignore modules constructed in memory.
        return;

    auto v = VisitorPreCompilation(this, unit->id(), unit->path());
    for ( auto i : v.walk(unit->module()) )
        v.dispatch(i);

    for ( const auto& e : v.enums ) {
        if ( e.id.namespace_() == ID("hilti") || e.id.namespace_() == ID("spicy_rt") )
            continue;

        ZEEK_DEBUG(hilti::util::fmt("  Got public enum type '%s'", e.id));
        hookNewEnumType(e);
        _enums.push_back(e);
    }
}

void Driver::hookNewASTPostCompilation(std::shared_ptr<hilti::Unit> unit) {
    if ( unit->extension() != ".spicy" )
        return;

    if ( unit->id() == ID("spicy_rt") || unit->id() == ID("hilti") || unit->id() == ID("zeek_rt") ||
         unit->id() == ID("zeek") )
        return;

    if ( unit->path().empty() )
        // Ignore modules constructed in memory.
        return;

    auto v = VisitorPostCompilation(this, unit->id(), unit->path());
    for ( auto i : v.walk(unit->module()) )
        v.dispatch(i);

    for ( auto&& u : v.units ) {
        ZEEK_DEBUG(hilti::util::fmt("  Got unit type '%s'", u.id));
        hookNewUnitType(u);
        _units[u.id] = std::move(u);
    }

    _glue->addSpicyModule(unit->id(), unit->path());
}

hilti::Result<hilti::Nothing> Driver::hookCompilationFinished(const hilti::Plugin& plugin) {
    if ( ! _need_glue )
        return hilti::Nothing();

    _need_glue = false;

    if ( _glue->compile() )
        return hilti::Nothing();
    else
        return hilti::result::Error("glue compilation failed");
}

void Driver::hookInitRuntime() { spicy::rt::init(); }

void Driver::hookFinishRuntime() { spicy::rt::done(); }
