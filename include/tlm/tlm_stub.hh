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

// ---- tlm_array<T> (Accellera tlm_array.h pattern: private inherit std::vector) ----
// Must be defined BEFORE tlm_generic_payload because tlm_generic_payload
// holds a tlm_array<> member by value.
template <typename T>
class tlm_array : private std::vector<T> {
public:
    using size_type = typename std::vector<T>::size_type;
    using std::vector<T>::vector;  // inherit constructors
    // Expose iterator / clear to support range-for and m_extensions.clear().
    using std::vector<T>::begin;
    using std::vector<T>::end;
    using std::vector<T>::clear;

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

class tlm_generic_payload {
private:
    tlm_command cmd;
    uint64_t addr;
    uint8_t* data;
    unsigned int len;
    tlm_response_status response_status;
    // Phase 1c: multi-extension array indexed by tlm_extension<T>::ID.
    // Replaces single-pointer `ext` member. Thread-safe via mutex.
    tlm_array<tlm_extension_base*> m_extensions;
    mutable std::mutex m_extensions_mutex;

    // Internal helper: grow m_extensions so that index `id` is in range.
    // Caller must hold m_extensions_mutex.
    void resize_for(unsigned int id) {
        if (id >= m_extensions.size()) m_extensions.expand(id + 1);
    }

public:
    tlm_generic_payload()
        : cmd(TLM_IGNORE_COMMAND), addr(0), data(nullptr), len(0),
          response_status(TLM_OK_RESPONSE), m_extensions() {}
    ~tlm_generic_payload() {
        delete[] data;
        // Phase 1c minimal dtor: loop-delete all owned extensions.
        // Phase 1d may refine the order/exception-safety here.
        for (auto& e : m_extensions) delete e;
    }

    void reset() {
        cmd = TLM_IGNORE_COMMAND; addr = 0;
        delete[] data; data = nullptr; len = 0;
        response_status = TLM_OK_RESPONSE;
        // Phase 1c minimal reset: drop ownership without delete.
        // Phase 1d will re-implement full multi-extension cleanup
        // (loop-delete + nullify) — see plan section "Phase 1d".
        std::lock_guard<std::mutex> lock(m_extensions_mutex);
        m_extensions.clear();
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

    // ===================== Extension API (Phase 1c multi-ext) =====================
    //
    // Modeled on SystemC TLM 2.0 (Accellera tlm_gp.cpp). Extensions are
    // stored in m_extensions[] indexed by T::ID (a per-type unique integer
    // allocated by tlm_extension_registry at static-init time).

    /// CALLER OWNS RETURNED POINTER:
    /// set_extension<T> stores `e` at index T::ID and returns the previously
    /// stored extension pointer (may be nullptr). Per SystemC TLM 2.0
    /// semantics, the CALLER is responsible for deleting the returned old
    /// pointer to avoid memory leaks. If you don't need the old extension,
    /// use clear_extension<T>() (drops ownership) or release_extension<T>()
    /// (deletes + drops ownership) instead.
    template<typename T> T* set_extension(T* e) {
        std::lock_guard<std::mutex> lock(m_extensions_mutex);
        resize_for(T::ID);
        T* old = static_cast<T*>(m_extensions[T::ID]);
        m_extensions[T::ID] = e;
        return old;
    }
    template<typename T> T* get_extension() const {
        std::lock_guard<std::mutex> lock(m_extensions_mutex);
        if (T::ID >= m_extensions.size()) return nullptr;
        return static_cast<T*>(m_extensions[T::ID]);
    }
    template<typename T> void get_extension(T*& e) const {
        std::lock_guard<std::mutex> lock(m_extensions_mutex);
        e = (T::ID < m_extensions.size()) ? static_cast<T*>(m_extensions[T::ID]) : nullptr;
    }
    /// Drop ownership of the T-typed extension (sets slot to nullptr).
    /// Does NOT delete the extension — caller is responsible.
    template<typename T> void clear_extension() {
        std::lock_guard<std::mutex> lock(m_extensions_mutex);
        if (T::ID < m_extensions.size()) m_extensions[T::ID] = nullptr;
    }
    /// Delete the T-typed extension and clear the slot.
    template<typename T> void release_extension() {
        std::lock_guard<std::mutex> lock(m_extensions_mutex);
        if (T::ID < m_extensions.size() && m_extensions[T::ID]) {
            delete m_extensions[T::ID];
            m_extensions[T::ID] = nullptr;
        }
    }
    /// Legacy bulk-clear: drop ownership of ALL extension slots (no delete).
    /// Retained for backward compatibility with old single-pointer API.
    void clear_extensions() {
        std::lock_guard<std::mutex> lock(m_extensions_mutex);
        for (auto& e : m_extensions) e = nullptr;
    }
    // =================== End Extension API =======================================

    bool is_dmi_allowed() const { return false; }
};

} // namespace tlm
#endif
