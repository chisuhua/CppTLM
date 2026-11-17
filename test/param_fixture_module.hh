// test/param_fixture_module.hh
// 参数加载机制的**测试夹具**模块（仅测试构建可见，不进生产代码）
// 作者 CppTLM Team / 日期 2026-11-17
//
// 用途（docs/adr/ADR-X.17-module-param-read-declares.md V-1 / V-2 / V-6）：
//   - 证明「读取即声明」机制本身存在且可红（RouterTLM 上的越界/类型错已被既有实现满足 ⇒ 无判别力）
//   - 证明「声明的默认值确实落到成员」且「参数值确实影响可观测行为」
//
// ⚠️ 成员初值**故意不等于**声明默认值（99 / 999）：
//    若默认值没有落到成员，`gain()` / `offset()` 会返回 99 / 999 ⇒ 测试立刻红。
#ifndef TEST_PARAM_FIXTURE_MODULE_HH
#define TEST_PARAM_FIXTURE_MODULE_HH

#include "core/sim_object.hh"

/// 2 个标量参数：
///   gain   : unsigned, 1..16,   默认 2   （覆盖无符号分支）
///   offset : int,      -64..64, 默认 0   （覆盖有符号分支）
/// 两者都参与 `compute()` ⇒ 「参数值不同 ⇒ 可观测指标不同」是真实语义，不是造出来的观测面。
class ParamFixtureTLM : public SimObject {
public:
    static constexpr const char* kType = "ParamFixtureTLM";

    ParamFixtureTLM(const std::string& n, EventQueue* eq) : SimObject(n, eq) {}

    void on_config_loaded() override {
        load_param("gain", gain_, 1u, 16u, 2u);
        load_param("offset", offset_, -64, 64, 0);
    }

    void tick() override {} // 本夹具只验证参数加载，不建模时序

    std::string get_module_type() const override { return kType; }

    unsigned gain() const { return gain_; }
    int offset() const { return offset_; }

    /// 被参数驱动的可观测行为
    long long compute(long long in) const {
        return in * static_cast<long long>(gain_) + static_cast<long long>(offset_);
    }

private:
    unsigned gain_ = 99; // ⚠️ 故意非法且 ≠ 声明默认值(2) —— 默认值未落成员时测试可红
    int offset_ = 999;   // ⚠️ 同上（越界且 ≠ 默认 0）
};

#endif // TEST_PARAM_FIXTURE_MODULE_HH
