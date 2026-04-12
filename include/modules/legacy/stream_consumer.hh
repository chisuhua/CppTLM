// include/modules/stream_consumer.hh
#ifndef STREAM_CONSUMER_HH
#define STREAM_CONSUMER_HH

#include "../sim_object.hh"
#include "../packet.hh"

class StreamConsumer : public SimObject {
public:
    explicit StreamConsumer(const std::string& n, EventQueue* eq) : SimObject(n, eq) {
        getPortManager().addUpstreamPort(this, "in");
        getPortManager().addDownstreamPort(this, "credit_out");  // 返回信用
    }

    // 回调：处理流数据
    bool handleUpstreamRequest(Packet* pkt, int src_id) {
        auto& pm = getPortManager();
        if (pm.getUpstreamPorts().size() > 4) {  // 模拟缓冲区限制
            DPRINTF(STREAM, "[%s] Buffer full! Dropping stream %lu seq=%lu\n",
                    name.c_str(), pkt->stream_id, pkt->seq_num);
            delete pkt;
            return false;
        }

        DPRINTF(STREAM, "[%s] Received stream %lu pkt %lu\n",
                name.c_str(), pkt->stream_id, pkt->seq_num);

        // 模拟处理延迟后返回信用
        event_queue->schedule(new LambdaEvent([this, pkt]() {
            Packet* credit_pkt = new Packet(nullptr, event_queue->getCurrentCycle(), PKT_CREDIT_RETURN);
            credit_pkt->credits = 1;
            credit_pkt->stream_id = pkt->stream_id;

            if (!getPortManager().getDownstreamPorts().empty()) {
                getPortManager().getDownstreamPorts()[0]->send(credit_pkt);
            } else {
                delete credit_pkt;
            }
            delete pkt;
        }), 5);

        return true;
    }

    void tick() override {}
};

#endif // STREAM_CONSUMER_HH
