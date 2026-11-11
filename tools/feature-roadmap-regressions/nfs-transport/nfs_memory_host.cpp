// SPDX-License-Identifier: GPL-3.0-or-later
// HOST-ONLY implementation of the r1 transport contract over an in-memory
// synthetic share. Fixture support code; never linked into the application.
#include "nfs_memory_host.h"

#include <cstring>

namespace Eden::NfsTransport {
namespace {

// Shared by the session wrapper and every open handle: mirrors the real
// readers-retain-their-session ownership without changing the pure interface.
struct Body {
    MemoryHostTransport* host = nullptr;
    bool poisoned = false;

    struct Identity {
        std::uint64_t size;
        std::uint64_t mtime_ns;
        std::uint64_t identity;
    };
    // Frozen at Connect; every operation revalidates against it.
    std::map<std::string, Identity> snapshot;
    // Session-owned cache: (name -> offset -> bytes of that exact request).
    std::map<std::string, std::map<std::uint64_t, std::vector<std::uint8_t>>> cache;

    bool fail(ErrorCode* code, ErrorCode value) const {
        *code = value;
        return false;
    }
    bool poison(ErrorCode* code, ErrorCode value) {
        poisoned = true;  // permanent: nothing in this host ever clears it
        return fail(code, value);
    }
};

class Handle final : public File {
public:
    Handle(std::shared_ptr<Body> body, std::string name, Stat stat)
        : body_(std::move(body)), name_(std::move(name)), stat_(stat) {}

    bool Read(std::uint64_t offset, std::vector<std::uint8_t>* buffer,
              std::size_t* read_bytes, ErrorCode* code) override {
        *read_bytes = 0;
        *code = ErrorCode::Invalid;
        if (closed_) return body_->fail(code, ErrorCode::Invalid);
        if (body_->poisoned) return body_->fail(code, ErrorCode::Unavailable);
        if (body_->host->faults.next_timeout) {
            body_->host->faults.next_timeout = false;
            return body_->poison(code, ErrorCode::Timeout);
        }
        const auto current = body_->host->files.find(name_);
        if (current == body_->host->files.end())
            return body_->poison(code, ErrorCode::Changed);  // known object vanished
        const bool changed = current->second.bytes.size() != stat_.size ||
                             current->second.mtime_ns != stat_.mtime_ns ||
                             current->second.identity != stat_.identity;
        if (changed) return body_->poison(code, ErrorCode::Changed);
        if (body_->host->faults.next_read_fail_after_prefix) {
            body_->host->faults.next_read_fail_after_prefix = false;
            const std::uint64_t prefix =
                static_cast<std::uint64_t>(buffer->size() / 2);
            if (prefix > 0 && stat_.size > offset) {
                const std::uint64_t count =
                    prefix < stat_.size - offset ? prefix : stat_.size - offset;
                std::memcpy(buffer->data(), current->second.bytes.data() + offset,
                            static_cast<std::size_t>(count));
            }
            return body_->poison(code, ErrorCode::Timeout);  // instability
        }
        if (offset > stat_.size) return body_->poison(code, ErrorCode::Changed);
        auto& entries = body_->cache[name_];
        const auto hit = entries.find(offset);
        if (hit != entries.end() && hit->second.size() == buffer->size()) {
            std::memcpy(buffer->data(), hit->second.data(), hit->second.size());
            *read_bytes = hit->second.size();
            return true;  // warm cache; unreachable once poisoned (checked above)
        }
        const std::uint64_t available = stat_.size - offset;
        const std::uint64_t want = static_cast<std::uint64_t>(buffer->size());
        const std::uint64_t count = want < available ? want : available;
        if (count > 0) {
            std::memcpy(buffer->data(), current->second.bytes.data() + offset,
                        static_cast<std::size_t>(count));
            if (count == want)
                entries[offset].assign(buffer->begin(), buffer->end());
        }
        *read_bytes = static_cast<std::size_t>(count);  // 0 at exact EOF or on a
                                                         // zero-length request
                                                         // (interface rule 6)
        return true;
    }

    Stat stat() const override { return stat_; }
    void Close() override { closed_ = true; }

private:
    std::shared_ptr<Body> body_;
    std::string name_;
    Stat stat_;
    bool closed_ = false;
};

class MemorySession final : public Session {
public:
    explicit MemorySession(std::shared_ptr<Body> body) : body_(std::move(body)) {}

    bool List(std::vector<std::string>* names, ErrorCode* code) override {
        names->clear();
        *code = ErrorCode::Invalid;
        if (body_->poisoned) return body_->fail(code, ErrorCode::Unavailable);
        if (body_->host->faults.next_timeout) {
            body_->host->faults.next_timeout = false;
            return body_->poison(code, ErrorCode::Timeout);
        }
        std::size_t seen = 0;
        for (const auto& entry : body_->host->files) {
            if (body_->host->faults.list_fail_halfway &&
                seen == body_->host->files.size() / 2)
                return body_->poison(code, ErrorCode::Timeout);  // midway instability
            const auto known = body_->snapshot.find(entry.first);
            if (known == body_->snapshot.end())
                return body_->poison(code, ErrorCode::Changed);  // namespace grew
            if (known->second.size != entry.second.bytes.size() ||
                known->second.mtime_ns != entry.second.mtime_ns ||
                known->second.identity != entry.second.identity)
                return body_->poison(code, ErrorCode::Changed);
            ++seen;
        }
        if (body_->snapshot.size() != body_->host->files.size())
            return body_->poison(code, ErrorCode::Changed);  // namespace shrank
        for (const auto& entry : body_->host->files) names->push_back(entry.first);
        return true;
    }

    // The base class member function name `Stat` hides the namespace-level
    // struct type `Stat` inside this derived-class scope (A50 r1 diagnostics,
    // nfs_memory_host.cpp:134-168). Qualify the type with its full namespace
    // so the override signature matches the interface EXACTLY; the denoted
    // type is unchanged.
    bool Stat(const std::string& name, Eden::NfsTransport::Stat* out,
              ErrorCode* code) override {
        *out = Eden::NfsTransport::Stat{};
        *code = ErrorCode::Invalid;
        if (body_->poisoned) return body_->fail(code, ErrorCode::Unavailable);
        if (body_->host->faults.next_timeout) {
            body_->host->faults.next_timeout = false;
            return body_->poison(code, ErrorCode::Timeout);
        }
        // Only names present in the connect-time snapshot are admitted. Any
        // other name — newly appeared or never seen — is a namespace change
        // and poisons the whole session (SMB-harmonized, A16 gate F2).
        const auto known = body_->snapshot.find(name);
        if (known == body_->snapshot.end())
            return body_->poison(code, ErrorCode::Changed);
        const auto current = body_->host->files.find(name);
        if (current == body_->host->files.end())
            return body_->poison(code, ErrorCode::Changed);  // known object gone
        if (known->second.size != current->second.bytes.size() ||
            known->second.mtime_ns != current->second.mtime_ns ||
            known->second.identity != current->second.identity)
            return body_->poison(code, ErrorCode::Changed);
        out->size = current->second.bytes.size();
        out->mtime_ns = current->second.mtime_ns;
        out->identity = current->second.identity;
        out->regular = true;
        return true;
    }

    bool Open(const std::string& name, std::unique_ptr<File>* out,
              ErrorCode* code) override {
        *code = ErrorCode::Invalid;
        if (out == nullptr) return body_->fail(code, ErrorCode::Invalid);
        Eden::NfsTransport::Stat admitted{};  // qualified: `Stat` hides the type here
        if (!Stat(name, &admitted, code)) return false;
        out->reset(new Handle(body_, name, admitted));
        return true;
    }

    bool Poisoned() const override { return body_->poisoned; }
    void Poison() override { body_->poisoned = true; }

private:
    std::shared_ptr<Body> body_;
};

}  // namespace

void MemoryHostTransport::AddFile(const std::string& name,
                                  std::vector<std::uint8_t> bytes,
                                  std::uint64_t mtime_ns, std::uint64_t identity) {
    MemoryFile file;
    file.bytes = std::move(bytes);
    file.mtime_ns = mtime_ns;
    file.identity = identity;
    files[name] = std::move(file);
}

void MemoryHostTransport::MutateFile(const std::string& name,
                                     std::vector<std::uint8_t> bytes) {
    const auto entry = files.find(name);
    if (entry == files.end()) return;
    entry->second.bytes = std::move(bytes);
    entry->second.mtime_ns += 1000;
    entry->second.identity += 1;
}

void MemoryHostTransport::MutateContentOnly(const std::string& name,
                                            std::vector<std::uint8_t> bytes) {
    const auto entry = files.find(name);
    if (entry == files.end()) return;
    entry->second.bytes = std::move(bytes);
}

void MemoryHostTransport::RemoveFile(const std::string& name) { files.erase(name); }

bool MemoryHostTransport::Connect(std::unique_ptr<Session>* out, ErrorCode* code) {
    if (out == nullptr) {
        if (code != nullptr) *code = ErrorCode::Invalid;
        return false;
    }
    auto body = std::make_shared<Body>();
    body->host = this;
    for (const auto& entry : files)
        body->snapshot[entry.first] = {entry.second.bytes.size(),
                                       entry.second.mtime_ns, entry.second.identity};
    out->reset(new MemorySession(std::move(body)));
    return true;
}

}  // namespace Eden::NfsTransport
