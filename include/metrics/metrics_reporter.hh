/**
 * @file metrics_reporter.hh
 * @brief MetricsReporter — 多格式统计报告生成器
 * 
 * 提供 Text / JSON / Markdown 三种格式的统计报告输出：
 * - TextReporter: gem5 风格对齐列格式
 * - JSONReporter: 嵌套 JSON 层次结构（使用 nlohmann/json）
 * - MarkdownReporter: Markdown 表格 + ASCII 直方图
 * 
 * 设计原则：
 * - 零外部依赖（JSON 除外，使用 header-only nlohmann/json）
 * - 头文件实现，无单独 .cc 源文件
 * - C++17 标准（std::filesystem 用于目录操作）
 * 
 * @author CppTLM Development Team
 * @date 2026-04-17
 */

#ifndef CPPTLM_METRICS_REPORTER_HH
#define CPPTLM_METRICS_REPORTER_HH

#include "metrics/stats.hh"
#include "metrics/histogram.hh"
#include "metrics/stats_manager.hh"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

// C++17: std::filesystem 用于目录创建
#include <filesystem>

// nlohmann/json header-only（已存在于 external/json/nlohmann/）
// include path: external/json → 解析为 nlohmann/json.hpp
#include "nlohmann/json.hpp"

namespace tlm_stats {

// ============================================================================
// MetricsReporter — 报告生成器基类
// ============================================================================

class MetricsReporter {
public:
    virtual ~MetricsReporter() = default;

    /**
     * @brief 生成报告到输出流
     * @param os 输出流
     */
    virtual void generate(std::ostream& os) const = 0;

    /**
     * @brief 生成报告到文件
     * @param path 文件路径
     * @return 成功返回 true
     */
    [[nodiscard]] virtual bool generate(const std::string& path) const {
        std::ofstream ofs(path);
        if (!ofs) return false;
        generate(ofs);
        ofs.close();
        return ofs.good();
    }

};

// ============================================================================
// TextReporter — gem5 风格文本格式
// ============================================================================

class TextReporter : public MetricsReporter {
public:
    explicit TextReporter(int width = 50) : width_(width) {}

    void generate(std::ostream& os) const override {
        os << "---------- Begin Simulation Statistics ----------\n";
        StatsManager::instance().dump_all(os, width_);
    }

    /// 生成报告到字符串
    [[nodiscard]]
    std::string generateToString() const {
        std::ostringstream oss;
        generate(oss);
        return oss.str();
    }

    /// 生成报告到文件
    bool generate(const std::string& path) const override {
        std::ofstream ofs(path);
        if (!ofs) return false;
        generate(ofs);
        ofs.close();
        return ofs.good();
    }

private:
    int width_;  // 字段宽度
};

// ============================================================================
// JSONReporter — 嵌套 JSON 格式
// ============================================================================

class JSONReporter : public MetricsReporter {
public:
    void generate(std::ostream& os) const override {
        nlohmann::json root = nlohmann::json::object();
        
        const auto& groups = StatsManager::instance().groups();
        for (const auto& kv : groups) {
            const std::string& path = kv.first;
            StatGroup* group = kv.second;
            if (!group) continue;

            // 将 "a.b.c" 解析为嵌套 JSON
            std::vector<std::string> parts;
            std::string remaining = path;
            size_t dot_pos;
            while ((dot_pos = remaining.find('.')) != std::string::npos) {
                parts.push_back(remaining.substr(0, dot_pos));
                remaining = remaining.substr(dot_pos + 1);
            }
            parts.push_back(remaining);

            // 在根 JSON 中创建嵌套路径
            nlohmann::json* current = &root;
            for (size_t i = 0; i < parts.size() - 1; ++i) {
                current = &((*current)[parts[i]]);
            }
            
            // 最后一个路径段作为 group 输出
            nlohmann::json group_json = nlohmann::json::object();
            dump_group_to_json(group_json, group);
            (*current)[parts.back()] = group_json;
        }

        os << root.dump(2);  // 格式化输出（2 空格缩进）
    }

    /// 生成报告到字符串
    [[nodiscard]]
    std::string generateToString() const {
        std::ostringstream oss;
        generate(oss);
        return oss.str();
    }

    /// 生成报告到文件
    bool generate(const std::string& path) const override {
        std::ofstream ofs(path);
        if (!ofs) return false;
        generate(ofs);
        ofs.close();
        return ofs.good();
    }

private:
    static void dump_group_to_json(nlohmann::json& j, const StatGroup* group) {
        if (!group) return;
        
        // 输出当前组的统计
        for (const auto& kv : group->stats()) {
            const std::string& stat_name = kv.first;
            const StatBase* stat = kv.second.get();
            
            if (auto* s = dynamic_cast<const Scalar*>(stat)) {
                j[stat_name] = s->value();
            } else if (auto* a = dynamic_cast<const Average*>(stat)) {
                j[stat_name] = a->result();
            } else if (auto* d = dynamic_cast<const Distribution*>(stat)) {
                nlohmann::json dist;
                dist["count"] = d->count();
                dist["min"] = d->min();
                dist["avg"] = d->mean();
                dist["max"] = d->max();
                dist["stddev"] = d->stddev();
                j[stat_name] = dist;
            } else if (auto* f = dynamic_cast<const Formula*>(stat)) {
                j[stat_name] = f->value();
            } else if (auto* p = dynamic_cast<const PercentileHistogram*>(stat)) {
                nlohmann::json pct;
                pct["count"] = p->total_count();
                pct["min"] = p->min_value();
                pct["p50"] = p->p50();
                pct["p95"] = p->p95();
                pct["p99"] = p->p99();
                pct["p99.9"] = p->p99_9();
                pct["max"] = p->max_value();
                j[stat_name] = pct;
            }
        }
        
        // 递归处理子组
        for (const auto& kv : group->subgroups()) {
            const std::string& subgroup_name = kv.first;
            const StatGroup* subgroup = kv.second.get();
            if (subgroup) {
                nlohmann::json subgroup_json = nlohmann::json::object();
                dump_group_to_json(subgroup_json, subgroup);
                j[subgroup_name] = subgroup_json;
            }
        }
    }
};

// ============================================================================
// MarkdownReporter — Markdown 表格格式
// ============================================================================

class MarkdownReporter : public MetricsReporter {
public:
    void generate(std::ostream& os) const override {
        os << "# Performance Metrics Report\n\n";
        StatsManager::instance().dump_all(os, 50);
        os << "\n\n";
        
        // 额外输出 JSON 格式摘要
        os << "## JSON Format\n\n```json\n";
        JSONReporter json_reporter;
        json_reporter.generate(os);
        os << "\n```\n";
    }

    /// 生成报告到字符串
    [[nodiscard]]
    std::string generateToString() const {
        std::ostringstream oss;
        generate(oss);
        return oss.str();
    }

    /// 生成报告到文件
    bool generate(const std::string& path) const override {
        std::ofstream ofs(path);
        if (!ofs) return false;
        generate(ofs);
        ofs.close();
        return ofs.good();
    }
};

// ============================================================================
// MultiReporter — 同时生成多种格式
// ============================================================================

class MultiReporter {
public:
    enum class Format { TEXT, JSON, MARKDOWN };

    void add_format(Format fmt, const std::string& path) {
        formats_.emplace_back(fmt, path);
    }

    /// 生成所有配置的格式
    bool generate_all() const {
        for (const auto& p : formats_) {
            if (!generate_one(p.first, p.second)) {
                return false;
            }
        }
        return true;
    }

    /// 生成到 output_dir，自动命名文件（C++17 std::filesystem 自动创建父目录）
    [[nodiscard]]
    bool generate_all(const std::string& output_dir) const {
        // C++17: std::filesystem::create_directories 会递归创建父目录
        std::filesystem::create_directories(output_dir);
        
        return generate_one(Format::TEXT, output_dir + "/metrics.txt")
            && generate_one(Format::JSON, output_dir + "/metrics.json")
            && generate_one(Format::MARKDOWN, output_dir + "/metrics.md");
    }

private:
    bool generate_one(Format fmt, const std::string& path) const {
        std::unique_ptr<MetricsReporter> reporter;
        switch (fmt) {
            case Format::TEXT:     reporter = std::make_unique<TextReporter>(); break;
            case Format::JSON:     reporter = std::make_unique<JSONReporter>(); break;
            case Format::MARKDOWN: reporter = std::make_unique<MarkdownReporter>(); break;
        }
        return reporter->generate(path);
    }

    std::vector<std::pair<Format, std::string>> formats_;
};

} // namespace tlm_stats

#endif // CPPTLM_METRICS_REPORTER_HH
