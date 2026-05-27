/**
 * @file topology_node.hh
 * @brief TGMS v4.0 Phase 4.1 层次结构解析器 - TopologyNode 类定义
 * @author OpenCode Team
 * @date 2026-05-27
 *
 * 功能说明:
 * - 定义 TopologyNode 类，表示层次结构中的一个节点
 * - 支持父子关系和 coherence domain
 */

#ifndef TOPOLOGY_NODE_HH
#define TOPOLOGY_NODE_HH

#include <memory>
#include <string>
#include <vector>

namespace cpptlm {

class TopologyNode {
public:
    using ChildPtr = std::shared_ptr<TopologyNode>;

    TopologyNode();
    explicit TopologyNode(const std::string& node_name);

    void add_child(ChildPtr child);
    std::vector<ChildPtr> get_children() const;
    bool has_coherence() const;
    std::string get_name() const;
    std::string get_parent() const;
    void set_parent(const std::string& parent);

private:
    std::string name_;
    std::string parent_name_;
    std::vector<ChildPtr> children_;
    std::string coherence_domain_;
};

}  // namespace cpptlm

#endif  // TOPOLOGY_NODE_HH