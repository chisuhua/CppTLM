// include/modules/stream_traffic_gen.hh
#ifndef STREAM_TRAFFIC_GEN_HH
#define STREAM_TRAFFIC_GEN_HH

#include "../sim_object.hh"
#include "../packet.hh"
#include "stream_producer.hh"

enum class TrafficMode {
    SEQUENTIAL,
    BURST,
    RANDOM
};

class StreamTrafficGenerator : public StreamProducer {
private:
    TrafficMode mode;
    uint64_t start_addr;
    uint64_t addr_step;

public:
    StreamTrafficGenerator(const std::string& n, EventQueue* eq)
        : StreamProducer(n, eq), mode(TrafficMode::SEQUENTIAL),
          start_addr(0x1000), addr_step(16) {}

    // 配置流量模式
    void configure(TrafficMode m, size_t packet_count, int reserved_credits = 2) {
        mode = m;
        createStream(packet_count, reserved_credits);
    }

    // 扩展发送逻辑（可重写）
    void tick() override {
        auto& ports = getPortManager().getDownstreamPorts();
        auto& streams_ = const_cast<std::vector<StreamProducer::StreamInfo>&>(streams);  // 访问 protected

        for (size_t i = 0; i < streams_.size(); ++i) {
            auto& stream = streams_[i];
            if (stream.sent_count >= stream.total_packets) continue;

            if (rand() % 5 == 0 && credit_pool->tryGetCredit(stream.stream_id)) {
                auto* trans = new tlm::tlm_generic_payload();
                trans->set_command(tlm::TLM_WRITE_COMMAND);

                uint32_t data = generateData(stream.stream_id, stream.sent_count);
                trans->set_data_ptr(reinterpret_cast<unsigned char*>(&data));
                trans->set_data_length(4);

                Packet* pkt = new Packet(trans, event_queue->getCurrentCycle(), PKT_STREAM_DATA);
                pkt->stream_id = stream.stream_id;
                pkt->seq_num = stream.sent_count;

                if (ports[i]->sendReq(pkt)) {
                    stream.sent_count++;
                    DPRINTF(TG, "[%s] Sent pkt %lu (data=0x%x)\n",
                            name.c_str(), pkt->seq_num, data);
                } else {
                    credit_pool->returnCredit(stream.stream_id);
                    delete pkt;
                }
            }
        }
    }

private:
    uint32_t generateData(uint64_t stream_id, size_t seq) {
        switch (mode) {
            case TrafficMode::SEQUENTIAL:
                return static_cast<uint32_t>(start_addr + seq * addr_step);
            case TrafficMode::BURST:
                return rand() % 2 ? 0xDEADBEEF : 0xC0DECAFE;
            case TrafficMode::RANDOM:
                return rand();
        }
        return 0;
    }
};

#endif // STREAM_TRAFFIC_GEN_HH
