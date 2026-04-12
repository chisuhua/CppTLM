class Crossbar_RR : public SimObject, public SlavePort {
private:
    struct OutputPort : public MasterPort {
        Crossbar_RR* owner;
        OutputPort(Crossbar_RR* o) : owner(o) {}
        bool recvResp(Packet* pkt) override {
            return owner->handleResponse(pkt);
        }
    };

    std::vector<OutputPort*> out_ports;
    int next_channel = 0;

public:
    Crossbar_RR(const std::string& n, EventQueue* eq) : SimObject(n, eq) {
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

    bool recvReq(Packet* pkt) override {
        MasterPort* dst = getNextPort();
        if (dst->sendReq(pkt)) {
            return true;
        }
        return false;
    }

    MasterPort* getNextPort() {
        MasterPort* p = out_ports[next_channel];
        next_channel = (next_channel + 1) % out_ports.size();
        return p;
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
