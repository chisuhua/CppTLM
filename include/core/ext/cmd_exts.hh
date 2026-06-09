// core/ext/cmd_exts.hh
//
// TLM 扩展宏定义（cmd_exts.hh）。
// 实际的扩展类（ReadCmdExt / WriteCmdExt / ...）已迁移到 ext/mem_exts.hh，
// 此处仅保留可复用的宏定义，便于后续添加新的扩展类型。
// ReqIDExt 是 cmd_exts.hh 独有的扩展（尚未在 mem_exts.hh 中迁移）。
//
// 历史：旧版 core/ext/cmd_exts.hh 同时定义宏和具体扩展类（ReadCmdExt 等），
// 但这些具体类与 ext/mem_exts.hh 中的定义重复。Phase 2 清理后，本文件
// 仅作为宏库存在，具体扩展类以 ext/mem_exts.hh 为准。
#ifndef CMD_EXTS_HH
#define CMD_EXTS_HH

#include "tlm/tlm_stub.hh"
#include <iostream>  // 用于 print
#include "cmd.hh"

#define GEMSC_TLM_EXTENSION_DEF(_name, _pod_type) \
    struct _name : public tlm::tlm_extension<_name> { \
        _pod_type data; \
        _name() = default; \
        explicit _name(const _pod_type& d) : data(d) {} \
        tlm::tlm_extension_base* clone() const override { \
            return new _name(*this); \
        } \
        void copy_from(tlm::tlm_extension_base const &e) override { \
            auto const* other = dynamic_cast<_name const*>(&e); \
            if (other) data = other->data; \
        } \
        void print(std::ostream& os = std::cout) const { \
            os << #_name " = " << data << std::endl; \
        } \
    };

#define GEMSC_TLM_EXTENSION_DEF_SIMPLE(_name, _type, _field_name) \
    struct _name : public tlm::tlm_extension<_name> { \
        _type _field_name; \
        _name() : _field_name(0) {} \
        explicit _name(_type val) : _field_name(val) {} \
        tlm::tlm_extension_base* clone() const override { \
            return new _name(*this); \
        } \
        void copy_from(tlm::tlm_extension_base const &e) override { \
            auto const* other = dynamic_cast<_name const*>(&e); \
            if (other) _field_name = other->_field_name; \
        } \
        void print(std::ostream& os = std::cout) const { \
            os << #_name " = " << _field_name << std::endl; \
        } \
    };

// ReqIDExt — cmd_exts.hh 独有的扩展（简单的 int32_t ID 字段）
// 其他扩展类（ReadCmdExt / WriteCmdExt / WriteDataExt / ReadRespExt /
// WriteRespExt / StreamIDExt）已统一在 ext/mem_exts.hh 中定义。
GEMSC_TLM_EXTENSION_DEF_SIMPLE(ReqIDExt, int32_t, req_id)


#endif // CMD_EXTS_HH
