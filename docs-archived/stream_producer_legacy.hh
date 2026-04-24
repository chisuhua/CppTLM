// include/modules/stream_producer.hh
#ifndef STREAM_PRODUCER_HH
#define STREAM_PRODUCER_HH

#include "../sim_object.hh"
#include "../packet.hh"
#include "../credit_pool.hh"

class StreamProducer : public SimObject {
protected:
    struct StreamInfo {
        uint64_t stream_id;
        size_t total_packets;
        size_t sent_count;
        int reserved_credits;

        StreamInfo(uint64_t id, size_t count, int credits)
            : stream_id(id), total_packets(count), sent_count(0), reserved_credits(credits) {}
    };

    std::unique_ptr<CreditPool> credit_pool;
    std::vector<StreamInfo> streams;
    uint64_t next_stream_id = 1001;

public:
    explicit StreamProducer(const std::string& n, EventQueue* eq)
        : SimObject(n, eq) {
        credit_pool = std::make_unique<CreditPool>(8);
    }

    // 创建新流（外部调用）
    void createStream(size_t packet_count, int reserved_credits = 2) {
        uint64_t sid = next_stream_id++;
        if (credit_pool->reserveCredits(sid, reserved_credits)) {
            streams.emplace_back(sid, packet_count, reserved_credits);
            getPortManager().addDownstreamPort(this, "stream_" + std::to_string(sid));
            DPRINTF(STREAM, "[%s] Created stream %lu (%zu pkts, %d reserved)\n",
                    name.c_str(), sid, packet_count, reserved_credits);
        } else {
            DPRINTF(STREAM, "[%s] Failed to reserve credits for stream %lu\n", name.c_str(), sid);
        }
    }

    // 回调：处理信用返回
    bool handleDownstreamResponse(Packet* pkt, int src_id) {
        if (pkt->isCredit()) {
            for (int i = 0; i < pkt->credits; ++i) {
                credit_pool->returnCredit(pkt->stream_id);
            }
            DPRINTF(CREDIT, "[%s] Received %d credits for stream %lu\n",
                    name.c_str(), pkt->credits, pkt->stream_id);
            delete pkt;
            return true;
        }
        return false;
    }

    void tick() override {
        auto& ports = getPortManager().getDownstreamPorts();
        for (size_t i = 0; i < streams.size(); ++i) {
            auto& stream = streams[i];
            if (stream.sent_count >= stream.total_packets) continue;

            // 1/5 概率尝试发送
            if (rand() % 5 == 0 && credit_pool->tryGetCredit(stream.stream_id)) {
                auto* trans = new tlm::tlm_generic_payload();
                trans->set_command(tlm::TLM_WRITE_COMMAND);
                uint32_t data = static_cast<uint32_t>(stream.sent_count);
                trans->set_data_ptr(reinterpret_cast<unsigned char*>(&data));
                trans->set_data_length(4);

                Packet* pkt = new Packet(trans, event_queue->getCurrentCycle(), PKT_STREAM_DATA);
                pkt->stream_id = stream.stream_id;
                pkt->seq_num = stream.sent_count;

                if (ports[i]->sendReq(pkt)) {
                    stream.sent_count++;
                    DPRINTF(STREAM, "[%s] Sent pkt %lu on stream %lu\n",
                            name.c_str(), pkt->seq_num, stream.stream_id);
                } else {
                    credit_pool->returnCredit(stream.stream_id);
                    delete pkt;
                }
            }
        }
    }

    const CreditPool& getCreditPool() const { return *credit_pool; }
};

#endif // STREAM_PRODUCER_HH
