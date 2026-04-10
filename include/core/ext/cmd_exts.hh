// extensions/mem_exts.hh
#ifndef MEM_EXTS_HH
#define MEM_EXTS_HH

#ifdef USE_SYSTEMC_STUB
#include "tlm/tlm_stub.hh"
#else
#include <tlm>
#endif
#include <iostream>  // 👈 用于 print
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

// --- 定义所有扩展 ---
GEMSC_TLM_EXTENSION_DEF(ReadCmdExt,   ReadCmd)
GEMSC_TLM_EXTENSION_DEF(WriteCmdExt,  WriteCmd)
GEMSC_TLM_EXTENSION_DEF(WriteDataExt, WriteData)
GEMSC_TLM_EXTENSION_DEF(ReadRespExt,  ReadResp)
GEMSC_TLM_EXTENSION_DEF(WriteRespExt, WriteResp)
GEMSC_TLM_EXTENSION_DEF(StreamIDExt,  StreamUniqID)
GEMSC_TLM_EXTENSION_DEF_SIMPLE(ReqIDExt, int32_t, req_id)


#endif // MEM_EXTS_HH
