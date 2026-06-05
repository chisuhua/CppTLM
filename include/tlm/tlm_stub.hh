// tlm_stub.hh - 轻量级 TLM 2.0 存根（避免链接 SystemC 库）
// Stub of SystemC TLM 2.0 tlm_generic_payload. Extension instances must NOT be
// namespace-scope globals; use function-local statics.
#ifndef TLM_STUB_HH
#define TLM_STUB_HH
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

namespace tlm {
// Meyers-singleton extension registry (Accellera tlm_gp.cpp pattern).
namespace {
class tlm_extension_registry {
public:
    static tlm_extension_registry& instance() { static tlm_extension_registry inst; return inst; }
    unsigned int register_extension(const std::type_info& type) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = map_.find(std::type_index(type));
        if (it != map_.end()) return it->second;
        unsigned int id = next_id_++;
        map_[std::type_index(type)] = id;
        return id;
    }
    unsigned int max_num_extensions() const { std::lock_guard<std::mutex> lock(mutex_); return next_id_; }
private:
    tlm_extension_registry() = default;
    tlm_extension_registry(const tlm_extension_registry&) = delete;
    tlm_extension_registry& operator=(const tlm_extension_registry&) = delete;
    std::unordered_map<std::type_index, unsigned int> map_;
    unsigned int next_id_ = 0;
    mutable std::mutex mutex_;
};
}  // anonymous namespace

// Public API: query number of registered extension types.
inline unsigned int max_num_extensions() { return tlm_extension_registry::instance().max_num_extensions(); }

#ifndef TLM_COMMAND_H
#define TLM_COMMAND_H
    enum tlm_command { TLM_IGNORE_COMMAND = 0, TLM_READ_COMMAND, TLM_WRITE_COMMAND };
#endif

#ifndef TLM_RESPONSE_STATUS_H
#define TLM_RESPONSE_STATUS_H
    enum tlm_response_status {
        TLM_OK_RESPONSE = 0, TLM_INCOMPLETE_RESPONSE,
        TLM_GENERIC_ERROR_RESPONSE, TLM_ADDRESS_ERROR_RESPONSE, TLM_COMMAND_ERROR_RESPONSE
    };
#endif

#ifndef TLM_EXTENSION_BASE_H
#define TLM_EXTENSION_BASE_H
    class tlm_extension_base {
    public:
        virtual ~tlm_extension_base() = default;
        virtual tlm_extension_base* clone() const = 0;
        virtual void copy_from(const tlm_extension_base& ext) = 0;
        virtual unsigned int get_extension_id() const { return 0; }
        // Thread-safe singleton-based registration. Single source of truth for
        // extension type IDs across all translation units.
        static unsigned int register_extension_s(const std::type_info& type) {
            return tlm_extension_registry::instance().register_extension(type);
        }
    protected:
        tlm_extension_base() = default;
    };

    template<typename T>
    class tlm_extension : public tlm_extension_base {
    public:
        /// Compile-time unique ID per extension type. C++11 guarantees thread-safe
        /// initialization of class static members, so this is safe to access from
        /// multiple TUs (unlike the previous function-local static).
        static const unsigned int ID;
        tlm_extension() = default;
        tlm_extension_base* clone() const override = 0;
        void copy_from(const tlm_extension_base& ext) override = 0;
        unsigned int get_extension_id() const override { return ID; }
    };
    // Out-of-class definition: triggers singleton registration exactly once per
    // extension type T (per program, not per TU).
    template<typename T>
    const unsigned int tlm_extension<T>::ID = tlm_extension_base::register_extension_s(typeid(T));
#endif

class tlm_generic_payload {
private:
    tlm_command cmd;
    uint64_t addr;
    uint8_t* data;
    unsigned int len;
    tlm_response_status response_status;
    tlm_extension_base* ext;
public:
    tlm_generic_payload() : cmd(TLM_IGNORE_COMMAND), addr(0), data(nullptr), len(0), response_status(TLM_OK_RESPONSE), ext(nullptr) {}
    ~tlm_generic_payload() { delete[] data; delete ext; }
    
    void reset() {
        cmd = TLM_IGNORE_COMMAND; addr = 0;
        delete[] data; data = nullptr; len = 0;
        response_status = TLM_OK_RESPONSE;
        delete ext; ext = nullptr;
    }
    
    tlm_command get_command() const { return cmd; }
    void set_command(tlm_command c) { cmd = c; }
    uint64_t get_address() const { return addr; }
    void set_address(uint64_t a) { addr = a; }
    unsigned int get_data_length() const { return len; }
    void set_data_length(unsigned int l) { delete[] data; data = new uint8_t[l]; memset(data, 0, l); len = l; }
    uint8_t* get_data_ptr() { return data; }
    const uint8_t* get_data_ptr() const { return data; }
    void set_data_ptr(uint8_t* d) { data = d; }
    tlm_response_status get_response_status() const { return response_status; }
    void set_response_status(tlm_response_status s) { response_status = s; }
    std::string get_response_string() const { return "OK"; }
    
    template<typename T> T* get_extension() const { return dynamic_cast<T*>(ext); }
    template<typename T> bool get_extension(T*& e) const { e = dynamic_cast<T*>(ext); return (e != nullptr); }
    template<typename T> void set_extension(T* e) { delete ext; ext = e; }
    template<typename T> void clear_extension() { delete ext; ext = nullptr; }
    void clear_extensions() { delete ext; ext = nullptr; }
    bool is_dmi_allowed() const { return false; }
};

// ---- tlm_array<T> (Accellera tlm_array.h pattern: private inherit std::vector) ----
template <typename T = tlm_extension_base*>
class tlm_array : private std::vector<T> {
public:
    using size_type = typename std::vector<T>::size_type;
    using std::vector<T>::vector;  // inherit constructors

    typename std::vector<T>::reference operator[](size_type i) {
        return std::vector<T>::operator[](i);
    }
    typename std::vector<T>::const_reference operator[](size_type i) const {
        return std::vector<T>::operator[](i);
    }
    size_type size() const { return std::vector<T>::size(); }
    void expand(size_type new_size) {
        if (new_size > size()) this->resize(new_size);
    }
};

} // namespace tlm
#endif
