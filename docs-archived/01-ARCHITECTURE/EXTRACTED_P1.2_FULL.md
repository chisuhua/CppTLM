# P1.2 完整讨论内容

> 从会话文件中提取
> 消息数: 1, 总字符: 3492

---

## 消息 1

### 2. 现有端口设计分析

**当前结构**:
```
SimplePort (基类)
├─ send(Packet*) → 调用对端 recv()
└─ recv(Packet*) = 0 (纯虚)

MasterPort : SimplePort
├─ sendReq(Packet*) → 发送请求
└─ recvResp(Packet*) = 0 (接收响应)

SlavePort : SimplePort
├─ recv(Packet*) → 接收请求
└─ sendResp(Packet*) → 发送响应
```

**问题**:
- ❌ 仅支持 `Packet*`，无法扩展到其他类型
- ❌ 无背压语义（`bool` 返回但未标准化）
- ❌ 无生命周期钩子（preSend/postSend 等）

---

### 3. Port<T> 设计方案

#### 3.1 核心 API

```cpp
// include/core/port.hh
#ifndef PORT_HH
#define PORT_HH

#include <optional>
#include <string>

namespace gemsc {

// 泛型端口模板
template <typename T>
class Port {
protected:
    std::string name;
    bool connected = false;
    
public:
    explicit Port(const std::string& n = "") : name(n) {}
    virtual ~Port() = default;
    
    // === 核心接口：非阻塞，支持背压 ===
    
    // 发送数据（返回 false 表示背压）
    virtual bool trySend(const T& data) = 0;
    
    // 接收数据（无数据返回 nullopt）
    virtual std::optional<T> tryRecv() = 0;
    
    // === 生命周期钩子（可选覆盖）===
    virtual void preSend(const T& data) {}
    virtual void postSend(const T& data) {}
    virtual void preRecv() {}
    virtual void postRecv(const T& data) {}
    
    // === 状态查询 ===
    bool isConnected() const { return connected; }
    const std::string& getName() const { return name; }
    
    // === 连接管理（由 PortManager 调用）===
    virtual void connect(Port<T>* other) = 0;
};

} // namespace gemsc

#endif // PORT_HH
```

---

#### 3.2 PacketPort 包装器（向后兼容）

```cpp
// include/core/packet_port.hh
#ifndef PACKET_PORT_HH
#define PACKET_PORT_HH

#include "port.hh"
#include "simple_port.hh"

namespace gemsc {

// Packet 专用端口实现
class PacketPort : public Port<Packet*> {
private:
    SimplePort* legacy_port;  // 现有 SimplePort
    
public:
    explicit PacketPort(SimplePort* port) : legacy_port(port) {}
    
    // trySend: 非阻塞，背压时返回 false
    bool trySend(Packet* pkt) override {
        preSend(pkt);
        bool result = legacy_port->send(pkt);  // 现有 send() 返回 bool
        postSend(pkt);
        return result;
    }
    
    // tryRecv: 轮询模式（现有设计无队列）
    std::optional<Packet*> tryRecv() override {
        preRecv();
        // 现有设计：recv() 是回调模式，不是轮询
        // 需要适配层...
        return std::nullopt;  // 简化处理
    }
    
    void connect(Port<Packet*>* other) override {
        // PortPair 已处理连接
        connected = true;
    }
};

} // namespace gemsc

#endif // PACKET_PORT_HH
```

---

#### 3.3 新模块使用示例

```cpp
// 新模块：使用 Port<T>
class NewCacheModule : public SimObject {
private:
    gemsc::Port<Request>* req_port;
    gemsc::Port<Response>* resp_port;
    
public:
    bool handleUpstreamRequest(Packet* pkt) override {
        // 通过 Port<T> 发送
        Request req{pkt->addr, pkt->type};
        if (!req_port->trySend(req)) {
            // 背压处理
            return false;
        }
        return true;
    }
};
```

---

### 4. 兼容策略

| 模块类型 | 端口使用 | 迁移策略 |
|---------|---------|---------|
| **现有模块** | SimplePort | 保持不变 |
| **新模块（TLM）** | Port<T> | 使用新 API |
| **适配器模块** | PacketPort 包装 | 桥接新旧 |

---

### 5. 决策请求

**确认事项**:
- [ ] 同意 Port<T> 泛型模板设计
- [ ] 同意 PacketPort 包装器向后兼容
- [ ] 同意现有模块保持不变，新模块使用 Port<T>
- [ ] 同意 trySend/tryRecv 非阻塞语义

**老板，P1.1 是否确认？**

---

## P1.2: CppHDL 集成方案

### 1. 当前状态

✅ 已创建符号链接:
```
/workspace/CppTLM/external/CppHDL -> /workspace/CppHDL
```

### 2. 集成方案回顾

| 方案 | 优点 | 缺点 | 推荐阶段 |
|------|------|------|---------|
| **符号链接** | 简单、即时生效 | 无版本控制 | 开发阶段 ✅ |
| **Git Submodule** | 版本锁定、CI/CD 友好 | 需额外 git 命令 | 生产阶段 |
| **ExternalProject** | 完全自动化 | 构建时间增加 | 库依赖管理 |

### 3. CppHDL 头文件确认



---

