class Router_Hash : public SimObject, public SlavePort {
private:
    struct OutputPort : public MasterPort {
        Router_Hash* owner;
        OutputPort(Router_Hash* o) : owner(o) {}
        bool recvResp(Packet* pkt) override {
            return owner->handleResponse(pkt);
        }
    };

    std::vector<OutputPort*> out_ports;
    SlavePort* mem_target;

public:
    Router_Hash(const std::string& n, EventQueue* eq) : SimObject(n, eq) {
        for (int i = 0; i < 4; i++) {
            out_ports.push_back(new OutputPort(this));
        }
    }

    SlavePort* getInPort() { return this; }
    std::vector<MasterPort*> getOutPorts() {
        std::vector<MasterPort*> ports;
        for (auto* p : out_ports) ports.push_back(p);
        return ports;
    }

    void setMemoryTarget(SlavePort* mem) {
        mem_target = mem;
    }

    bool recvReq(Packet* pkt) override {
        MasterPort* dst = route(pkt);
        if (dst->sendReq(pkt)) {
            return true;
        }
        return false;
    }

    MasterPort* route(Packet* pkt) {
        uint64_t addr = pkt->payload->get_address();
        return out_ports[(addr >> 12) & 0x3];  // 哈希到 4 个 Cache
    }

    bool handleResponse(Packet* pkt) {
        Packet* orig_req = pkt->original_req;
        event_queue->schedule(new LambdaEvent([this, orig_req]() {
            getInPort()->sendResp(orig_req);
        }), 1);
        delete pkt;
        return true;
    }
};
