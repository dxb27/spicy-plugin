// Copyright (c) 2020-2021 by the Zeek Project. See LICENSE for details.

#pragma once

#include <compiler/driver.h>

#include <memory>
#include <string>
#include <vector>

#include <zeek-spicy/zeek-compat.h>

#include <zeek/plugin/Plugin.h>

namespace spicy::rt {
struct Parser;
}

namespace plugin::Zeek_Spicy {

/** Customized Spicy-to-Zeek Driver class. */
class Driver : public spicy::zeek::Driver {
public:
    using spicy::zeek::Driver::Driver;

    // Called from plugin::Zeek_Spicy::Plugin, with same semantics.
    void InitPreScript();

    // Called from plugin::Zeek_Spicy::Plugin, with same semantics.
    void InitPostScript();

    // Called from plugin::Zeek_Spicy::Plugin, with same semantics.
    int HookLoadFile(const ::zeek::plugin::Plugin::LoadType type, const std::string& file, const std::string& resolved);

    // Called from plugin::Zeek_Spicy::Plugin, with same semantics.
    void addLibraryPaths(const std::string& dirs);

protected:
    /** Overridden from Spicy driver class. */
    void hookAddInput(const hilti::rt::filesystem::path& path) override;

    /** Overridden from Spicy driver class. */
    void hookAddInput(std::shared_ptr<hilti::Unit> unit) override;

    /** Overridden from Spicy driver class. */
    void hookNewEnumType(const spicy::zeek::EnumInfo& e) override;

private:
    friend class Plugin;
    void _initialize();

    bool _initialized = false;
    std::vector<hilti::rt::filesystem::path> _import_paths;
};

} // namespace plugin::Zeek_Spicy
