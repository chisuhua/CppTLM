/**
 * @file topology_node.cc
 * @brief TopologyNode 类实现
 */

#include "core/topology_node.hh"

namespace cpptlm {

    TopologyNode::TopologyNode() {
    }

    TopologyNode::TopologyNode(const std::string& node_name) : name_(node_name) {
    }

    void TopologyNode::add_child(ChildPtr child) {
        if (child) {
            child->set_parent(name_);
            children_.push_back(child);
        }
    }

    std::vector<TopologyNode::ChildPtr> TopologyNode::get_children() const {
        return children_;
    }

    bool TopologyNode::has_coherence() const {
        return !coherence_domain_.empty();
    }

    std::string TopologyNode::get_name() const {
        return name_;
    }

    std::string TopologyNode::get_parent() const {
        return parent_name_;
    }

    void TopologyNode::set_parent(const std::string& parent) {
        parent_name_ = parent;
    }

} // namespace cpptlm