// test/test_cep_version_change_traceability.cc
// `INV-CEP-11-IC3` —— 规范版本变更**必须**在台账留 `scope-change` / `adr-amendment` entry
//
// 判据来源:per `ADR-CEP-11 §4` 的 IC3(原文:「规范版本变更**必须**在
//   `objective-cep-platform §11` 台账留 `scope-change` / `adr-amendment` entry」)
//   + `ADR-CEP-11 §5` 抓取点 3(「变更留痕:锁定表变更**必须**伴随台账 entry」)
//
// 机器化口径(为什么这样可判 —— 锚点是 ADR 自身的「增补块」规矩):
//   · `ADR-CEP-11 §3` 是**唯一真源**的锁表;ADR 不可变 ⇒ 表内变更走**原地更新**,
//     且**必须**在 §3 段内留一个「**增补块**」(blockquote,含 `per 台账 Entry #N` 授权锚);
//   · ⇒ 「锁表变更 ⇒ 有增补块 ⇒ 增补块的授权 entry 在台账里带合规标签」;
//   · 且增补块声明的**新版本取值必须落在当前锁表内** ⇒ 表与增补块不得漂移(双真源防护)。
//
// ⚠️ **不是**文本存在性断言(按 architect 判据 1.1:「ADR 里写着这句话」不咬合语义)
//   —— 变红条件:① 增补块的授权 entry 缺失 / 标签非 {scope-change, adr-amendment}
//              ② 增补块声明的新值不在当前锁表内(表与增补块漂移)
//   ⇒ 判据**直接读台账**,而不是读 ADR 的关键词。
//
// 作者: CppTLM Team / 日期: 2026-11-05
#include "catch_amalgamated.hpp"

#include "cep_adr_parser.hh"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using cpptlm::cep::parse_protocol_version_lock_table;
using cpptlm::cep::read_file;
using cpptlm::cep::repo_root;

namespace {

    constexpr const char* kLedgerRel = ".rddf/roadmap/objectives/obj-cep-platform.md";
    constexpr const char* kAdrRel = "docs/adr/ADR-CEP-11-protocol-version-locking.md";
    constexpr const char* kLockSectionHead = "## 3. 规范版本锁定表";

    // 合规标签白名单(per IC3 原文)
    const std::set<std::string> kTraceTags{"scope-change", "adr-amendment"};

    // ── 台账 entry 头解析:`### Entry #123 — 2026-11-05 [scope-change](标题)` ──
    std::map<std::string, std::string> parse_entry_tags(const std::string& ledger) {
        std::map<std::string, std::string> out;
        std::istringstream ss(ledger);
        std::string line;
        while (std::getline(ss, line)) {
            static const std::string kPrefix = "### Entry #";
            if (line.rfind(kPrefix, 0) != 0)
                continue;
            std::string id = "Entry #";
            std::size_t i = kPrefix.size();
            while (i < line.size() && std::isdigit(static_cast<unsigned char>(line[i])))
                id += line[i++];
            if (id == "Entry #")
                continue; // 无编号
            const auto lb = line.find('[', i);
            const auto rb = (lb == std::string::npos) ? std::string::npos : line.find(']', lb);
            if (lb == std::string::npos || rb == std::string::npos)
                continue;
            out[id] = line.substr(lb + 1, rb - lb - 1);
        }
        return out;
    }

    // ── §3 锁表段的正文(从段头到下一个 `## `)───────────────────────────
    std::string lock_section_body(const std::string& adr) {
        const auto sec = adr.find(kLockSectionHead);
        if (sec == std::string::npos)
            return {};
        const auto sec_end = adr.find("\n## ", sec + 1);
        return adr.substr(sec, (sec_end == std::string::npos ? adr.size() : sec_end) - sec);
    }

    // ── 增补块解析:blockquote(`> `)连续块,含「增补块」关键词 ─────────────
    struct SupplementBlock {
        std::string text;
        std::string entry_id;              // "Entry #135"(空 = 块内无授权锚)
        std::vector<std::string> literals; // 块内反引号包裹的版本字面量
        std::string new_value;             // 块内声明的**新**取值(「更新为 X」之 X)
    };

    std::vector<SupplementBlock> parse_supplement_blocks(const std::string& section) {
        std::vector<SupplementBlock> out;
        std::istringstream ss(section);
        std::string line;
        SupplementBlock cur;
        bool in_block = false;
        auto flush = [&]() {
            if (in_block && !cur.text.empty())
                out.push_back(cur);
            cur = SupplementBlock{};
            in_block = false;
        };
        while (std::getline(ss, line)) {
            const bool quote = line.rfind("> ", 0) == 0 || line.rfind(">", 0) == 0;
            if (quote && line.find("增补块") != std::string::npos) {
                in_block = true;
                cur.text += line + "\n";
                continue;
            }
            if (in_block && quote && !line.empty()) {
                cur.text += line + "\n";
                continue;
            }
            flush();
        }
        flush();

        for (auto& b : out) {
            const auto e = b.text.find("Entry #");
            if (e != std::string::npos) {
                std::string id = "Entry #";
                std::size_t i = e + 7;
                while (i < b.text.size() && std::isdigit(static_cast<unsigned char>(b.text[i])))
                    id += b.text[i++];
                if (id != "Entry #")
                    b.entry_id = id;
            }
            // 反引号包裹的版本字面量:`chi-r2` / `tilelink-c-1.7.1` …
            std::size_t pos = 0;
            while ((pos = b.text.find('`', pos)) != std::string::npos) {
                const auto e2 = b.text.find('`', pos + 1);
                if (e2 == std::string::npos)
                    break;
                const std::string lit = b.text.substr(pos + 1, e2 - pos - 1);
                // 版本字面量形态:`<族>-<版本>`(含数字且含 '-';如 `tilelink-c-1.7.1`)
                const bool has_digit = lit.find_first_of("0123456789") != std::string::npos;
                if (has_digit && lit.find('-') != std::string::npos)
                    b.literals.push_back(lit);
                pos = e2 + 1;
            }
            // **新**取值 = 「更新为 / 改为 / 替换为」之后**第一个**反引号字面量
            // (归一化:去掉 blockquote 换行前缀,否则标记与字面量会被行尾隔开)
            std::string flat = b.text;
            for (const char* pat : {"\n> ", "\n>"}) {
                std::size_t p = 0;
                while ((p = flat.find(pat, p)) != std::string::npos)
                    flat.replace(p, std::string(pat).size(), " ");
            }
            static const char* kMarkers[] = {"原地更新**为", "原地更新为", "更新**为", "更新为",
                                             "改为",         "替换为",     "修订为"};
            for (const char* m : kMarkers) {
                const auto mp = flat.find(m);
                if (mp == std::string::npos)
                    continue;
                const auto lb = flat.find('`', mp + std::string(m).size());
                if (lb == std::string::npos)
                    continue;
                const auto rb = flat.find('`', lb + 1);
                if (rb == std::string::npos)
                    continue;
                b.new_value = flat.substr(lb + 1, rb - lb - 1);
                break;
            }
        }
        return out;
    }

    // ── 判定核心:增补块(锁表变更留痕锚)是否合规 ────────────────────────
    struct BlockVerdict {
        bool ok = false;
        std::string reason;
    };

    BlockVerdict check_supplement_block(const SupplementBlock& b,
                                        const std::vector<std::string>& lock_versions,
                                        const std::map<std::string, std::string>& entry_tags) {
        // ① 授权 entry 必须在台账中存在
        if (b.entry_id.empty())
            return {false, "增补块未携带授权 entry(留痕缺失)"};
        const auto it = entry_tags.find(b.entry_id);
        if (it == entry_tags.end())
            return {false, "台账中不存在授权 entry " + b.entry_id + "(留痕缺失)"};
        // ② 标签必须合规
        if (kTraceTags.count(it->second) == 0)
            return {false, "授权 entry " + b.entry_id + " 的台账标签为 [" + it->second +
                               "] ⇒ 不在 {scope-change, adr-amendment}"};
        // ③ 增补块声明的**新版本取值必须落在当前锁表内**(表 ↔ 增补块不得漂移)
        if (!b.new_value.empty()) {
            if (std::find(lock_versions.begin(), lock_versions.end(), b.new_value) ==
                lock_versions.end())
                return {false, "增补块声明的新版本取值 " + b.new_value +
                                   " 不在当前锁表内 ⇒ 表与增补块漂移"};
        } else {
            // 无「更新为」标记 ⇒ 退化为「块内至少一个取值在锁表内」
            bool any_in_table = false;
            for (const auto& lit : b.literals) {
                if (std::find(lock_versions.begin(), lock_versions.end(), lit) !=
                    lock_versions.end()) {
                    any_in_table = true;
                    break;
                }
            }
            if (!any_in_table)
                return {false, "增补块声明的版本取值均不在当前锁表内 ⇒ 表与增补块漂移"};
        }
        return {true, "授权 entry " + b.entry_id + " 标签 [" + it->second + "] 合规"};
    }

} // namespace

// INV-CARRIER: INV-CEP-11-IC3
// 变红理由(per ADR-CEP-06 SU#4 规则 B):锁表变更的增补块,其授权 entry 缺失或标签非
// scope-change/adr-amendment / 增补块新值不在锁表内 ⇒ 红
TEST_CASE("CEP VersionLock: IC3 —— 规范版本变更在台账留 scope-change/adr-amendment entry",
          "[cep][version-lock]") {
    const auto ledger = read_file((repo_root() / kLedgerRel).string());
    REQUIRE(!ledger.empty());
    const auto adr = read_file((repo_root() / kAdrRel).string());
    REQUIRE(!adr.empty());

    // 锁表 = 唯一真源(从 ADR §3 **解析**,不硬编码取值)
    const auto lock = parse_protocol_version_lock_table(adr);
    REQUIRE(lock.size() == 8u);
    const std::vector<std::string> lock_versions(lock.begin(), lock.end());

    const auto tags = parse_entry_tags(ledger);
    REQUIRE(tags.size() > 10u);

    const auto blocks = parse_supplement_blocks(lock_section_body(adr));
    // 当前至少 1 个增补块(否则本断言恒真 ⇒ 判据失效)
    REQUIRE(blocks.size() >= 1u);

    // ── 正控:每个增补块 ⇒ 授权 entry 留痕合规 + 新值在锁表内 ──────────────
    for (const auto& b : blocks) {
        const auto v = check_supplement_block(b, lock_versions, tags);
        INFO("锁表变更留痕不合规: " << v.reason);
        REQUIRE(v.ok);
    }

    // ── 反向 fixture(判别力证据)────────────────────────────────────────
    // ① 授权 entry 标签为 `build` ⇒ 必须判否
    {
        SupplementBlock bad;
        bad.text = "> ⚠️ **增补块(per 台账 Entry #999)**:本表某行由 `chi-r2` 原地更新为 `chi-r2`。";
        bad.entry_id = "Entry #999";
        bad.literals = {"chi-r2"};
        bad.new_value = "chi-r2";
        std::map<std::string, std::string> fake{{"Entry #999", "build"}};
        const auto v = check_supplement_block(bad, lock_versions, fake);
        INFO("反向 fixture 应判否,实际: " << v.reason);
        REQUIRE_FALSE(v.ok);
    }
    // ② 授权 entry 在台账中不存在 ⇒ 必须判否(留痕缺失)
    {
        SupplementBlock bad;
        bad.text = "> ⚠️ **增补块(per 台账 Entry #998)**:由 `chi-r2` 改为 `chi-r2`。";
        bad.entry_id = "Entry #998";
        bad.literals = {"chi-r2"};
        bad.new_value = "chi-r2";
        const auto v = check_supplement_block(bad, lock_versions, tags);
        INFO("反向 fixture 应判否,实际: " << v.reason);
        REQUIRE_FALSE(v.ok);
    }
    // ③ 增补块声明的**新值不在锁表内** ⇒ 必须判否(表 ↔ 增补块漂移)
    {
        SupplementBlock bad;
        bad.text = "> ⚠️ **增补块(per 台账 Entry #135)**:由 `chi-r2` 原地更新为 `chi-r9.9`。";
        bad.entry_id = "Entry #135";
        bad.literals = {"chi-r2", "chi-r9.9"};
        bad.new_value = "chi-r9.9";
        const auto v = check_supplement_block(bad, lock_versions, tags);
        INFO("反向 fixture 应判否,实际: " << v.reason);
        REQUIRE_FALSE(v.ok);
    }
    // ④ 标签合规 + 新值在表内 ⇒ 必须判否为「通过」(证明正向路径非恒假)
    {
        SupplementBlock good;
        good.text = "> ⚠️ **增补块(per 台账 Entry #135)**:由 `chi-r2` 原地更新为 `chi-r2`。";
        good.entry_id = "Entry #135";
        good.literals = {"chi-r2"};
        good.new_value = "chi-r2";
        const auto v = check_supplement_block(good, lock_versions, tags);
        REQUIRE(v.ok);
    }
}
