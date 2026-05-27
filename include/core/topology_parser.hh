/**
 * @file topology_parser.hh
 * @brief 层级拓扑树解析器 - 声明 parse_hierarchy_tree() 函数
 * @author OpenCode Team
 * @date 2026-05-27
 *
 * 功能说明:
 * - 提供 parse_hierarchy_tree() 函数，将 JSON hierarchy 对象解析为 TopologyNode 树结构
 * - 支持嵌套节点的递归解析
 * - 支持 coherence_domains 一致性域解析
 * - 检测循环引用并报告错误
 *
 * 依赖项:
 * - nlohmann::json for JSON parsing
 * - TopologyNode class for tree structure
 */

#ifndef TOPOLOGY_PARSER_HH
#define TOPOLOGY_PARSER_HH

#include "topology_node.hh"
#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace cpptlm {

class TopologyParseException : public std::exception {
public:
    explicit TopologyParseException(const std::string& message)
        : message_(message) {}

    const char* what() const noexcept override {
        return message_.c_str();
    }

private:
    std::string message_;
};

std::shared_ptr<TopologyNode> parse_hierarchy_tree(const nlohmann::json& hierarchy_json);

std::shared_ptr<TopologyNode>
parse_hierarchy_tree_with_validation(const nlohmann::json& hierarchy_json,
                                      const nlohmann::json& coherence_json = nlohmann::json::array());

std::vector<std::string> parse_coherence_domains_array(const nlohmann::json& coherence_json);

}  // namespace cpptlm

#endif  // TOPOLOGY_PARSER_HH