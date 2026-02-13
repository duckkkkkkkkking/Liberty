#include "LibertyParser.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include <boost/variant/get.hpp>

namespace {

bool parseLibertyFile(const std::string& lib_path,
                      liberty::ast::Library* out_library,
                      std::string* error)
{
    std::ifstream in(lib_path, std::fstream::in);
    if (!in.good()) {
        if (error != nullptr) {
            *error = "Cannot open liberty file: " + lib_path;
        }
        return false;
    }

    std::vector<char> buffer{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    std::replace(buffer.begin(), buffer.end(), '\n', ' ');
    std::replace(buffer.begin(), buffer.end(), '\\', ' ');

    auto first = buffer.begin();
    const bool ok = liberty::ast::liberty_parse(first, buffer.end(), *out_library);
    if (!ok) {
        if (error != nullptr) {
            *error = "liberty_parse failed for file: " + lib_path;
        }
        return false;
    }
    return true;
}

const liberty::ast::GroupStatement* findChildGroup(const liberty::ast::GroupStatement& parent,
                                                    const std::string& group_name,
                                                    const std::string& group_instance_name)
{
    for (const auto& child : parent.children) {
        auto group = boost::get<liberty::ast::GroupStatementAst>(&child);
        if (group == nullptr) {
            continue;
        }

        const auto& g = group->get();
        if (g.group_name == group_name && g.name == group_instance_name) {
            return &g;
        }
    }
    return nullptr;
}

std::optional<double> findSimpleNumericAttr(const liberty::ast::GroupStatement& group,
                                            const std::string& attr_name)
{
    for (const auto& child : group.children) {
        auto simple = boost::get<liberty::ast::SimpleAttribute>(&child);
        if (simple == nullptr || simple->name != attr_name) {
            continue;
        }

        if (const auto* v = boost::get<double>(&simple->value); v != nullptr) {
            return *v;
        }
        if (const auto* v = boost::get<int>(&simple->value); v != nullptr) {
            return static_cast<double>(*v);
        }
    }
    return std::nullopt;
}

}  // namespace

int main(int argc, char** argv)
{
    const std::string lib_path = (argc >= 2)
                                     ? argv[1]
                                     : "/Relocate_Sizing/LIB/asap7sc7p5t_AO_LVT_TT_nldm_211120.lib";
    const std::string cell_name = (argc >= 3)
                                      ? argv[2]
                                      : "A2O1A1Ixp33_ASAP7_75t_L";
    const std::string pin_name = (argc >= 4)
                                     ? argv[3]
                                     : "A1";

    liberty::ast::Library library;
    std::string error;
    if (!parseLibertyFile(lib_path, &library, &error)) {
        std::cerr << "[ERROR] " << error << '\n';
        return 1;
    }

    const auto& root = library.get();
    if (root.group_name != "library") {
        std::cerr << "[ERROR] Parsed root is not a library group.\n";
        return 2;
    }

    const auto* cell = findChildGroup(root, "cell", cell_name);
    if (cell == nullptr) {
        std::cerr << "[ERROR] Cell not found: " << cell_name << '\n';
        return 3;
    }

    const auto* pin = findChildGroup(*cell, "pin", pin_name);
    if (pin == nullptr) {
        std::cerr << "[ERROR] Pin not found in cell " << cell_name << ": " << pin_name << '\n';
        return 4;
    }

    const auto cap = findSimpleNumericAttr(*pin, "capacitance");
    if (!cap.has_value()) {
        std::cerr << "[ERROR] Attribute 'capacitance' not found on "
                  << cell_name << '/' << pin_name << '\n';
        return 5;
    }

    const auto rise_cap = findSimpleNumericAttr(*pin, "rise_capacitance");
    const auto fall_cap = findSimpleNumericAttr(*pin, "fall_capacitance");

    std::cout << "lib_file: " << lib_path << '\n';
    std::cout << "cell: " << cell_name << '\n';
    std::cout << "pin: " << pin_name << '\n';
    std::cout << "capacitance: " << *cap << '\n';
    if (rise_cap.has_value()) {
        std::cout << "rise_capacitance: " << *rise_cap << '\n';
    }
    if (fall_cap.has_value()) {
        std::cout << "fall_capacitance: " << *fall_cap << '\n';
    }

    return 0;
}
