// include/utils/json_includer.hh
#ifndef JSON_INCLUDER_HH
#define JSON_INCLUDER_HH

#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>

using json = nlohmann::json;

class JsonIncluder {
public:
    static json loadAndInclude(const std::string& filename) {
        std::ifstream f(filename);
        if (!f.is_open()) {
            throw std::runtime_error("Cannot open config file: " + filename);
        }
        std::stringstream buffer;
        buffer << f.rdbuf();
        json root = json::parse(buffer.str());

        processIncludes(root, filename);
        return root;
    }
    
    // 重载：直接处理 json 对象
    static json loadAndInclude(const json& config) {
        json result = config;
        processIncludesJson(result);
        return result;
    }

private:
    static void processIncludes(json& node, const std::string& base_path) {
        if (node.is_object()) {
            if (node.contains("include")) {
                std::string include_file = node["include"].get<std::string>();
                std::string dir = base_path.substr(0, base_path.find_last_of("/\\") + 1);
                json included = loadAndInclude(dir + include_file);

                for (auto& [key, value] : included.items()) {
                    if (!node.contains(key)) {
                        node[key] = value;
                    }
                }
                node.erase("include");
            }

            for (auto& [key, value] : node.items()) {
                processIncludes(value, base_path);
            }
        } else if (node.is_array()) {
            for (auto& item : node) {
                processIncludes(item, base_path);
            }
        }
    }
    
    // 处理 json 对象中的 include
    static void processIncludesJson(json& node) {
        if (node.is_object()) {
            if (node.contains("include")) {
                std::string include_file = node["include"].get<std::string>();
                json included = loadAndInclude(include_file);
                for (auto& [key, value] : included.items()) {
                    if (!node.contains(key)) node[key] = value;
                }
                node.erase("include");
            }
            for (auto& [key, value] : node.items()) {
                if (value.is_object() || value.is_array()) {
                    processIncludesJson(value);
                }
            }
        } else if (node.is_array()) {
            for (auto& item : node) {
                if (item.is_object() || item.is_array()) {
                    processIncludesJson(item);
                }
            }
        }
    }
};

#endif // JSON_INCLUDER_HH
