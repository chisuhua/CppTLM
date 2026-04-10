// include/extensions/qos_extension.hh
#ifndef QOS_EXTENSION_HH
#define QOS_EXTENSION_HH

#ifdef USE_SYSTEMC_STUB
#include "tlm/tlm_stub.hh"
#else
#include "tlm.h"
#endif

struct QoSExtension : public tlm::tlm_extension<QoSExtension> {
    uint8_t qos_class = 0;        // 0-7
    bool is_urgent = false;
    uint64_t deadline_cycle = 0;

    tlm_extension* clone() const override {
        return new QoSExtension(*this);
    }

    void copy_from(tlm_extension const &e) override {
        auto& ext = static_cast<const QoSExtension&>(e);
        qos_class = ext.qos_class;
        is_urgent = ext.is_urgent;
        deadline_cycle = ext.deadline_cycle;
    }
};

inline QoSExtension* get_qos(tlm::tlm_generic_payload* p) {
    QoSExtension* ext = nullptr;
    p->get_extension(ext);
    return ext;
}
