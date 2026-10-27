# SPDX-License-Identifier: GPL-3.0-or-later
# Small, checked derivatives of Eden 67bada77. Never replace the core with the Citron fork.
# Standalone generation is also used by tools/check-nextendo.py.
function(nextendo_replace variable before after expected)
    string(REPLACE "${before}" "" removed "${${variable}}")
    string(LENGTH "${${variable}}" original_size)
    string(LENGTH "${removed}" removed_size)
    string(LENGTH "${before}" match_size)
    math(EXPR count "(${original_size} - ${removed_size}) / ${match_size}")
    if(NOT count EQUAL expected)
        message(FATAL_ERROR "Pinned Nextendo overlay changed: ${variable}, expected ${expected}, found ${count}: ${before}")
    endif()
    string(REPLACE "${before}" "${after}" modified "${${variable}}")
    set(${variable} "${modified}" PARENT_SCOPE)
endfunction()
function(nextendo_write name content)
    file(WRITE "${NEXTENDO_OUTPUT}/${name}.in" "${content}")
    configure_file("${NEXTENDO_OUTPUT}/${name}.in" "${NEXTENDO_OUTPUT}/${name}" COPYONLY)
endfunction()
file(MAKE_DIRECTORY "${NEXTENDO_OUTPUT}")

file(READ "${NEXTENDO_SOURCE}/src/core/hle/service/sockets/sfdnsres.cpp" dns)
string(PREPEND dns "#include \"nextendo.h\"\n")
nextendo_replace(dns "const std::string host = Common::StringFromBuffer(host_buffer);"
    [=[const std::string host = Eden::Nextendo::GuestHost(Common::StringFromBuffer(host_buffer));
    if (host.empty()) return {0, GetAddrInfoError::AGAIN};]=] 2)
# Bypass the original official-host block only AFTER an exact route has become a Nextendo DNS
# name. Unmapped official hosts were rejected above; there is no official-service fallthrough.
nextendo_replace(dns "if (IsBlockedHost(host)) {"
    "if (IsBlockedHost(host) && !(Eden::Nextendo::GuestEnabled() && Eden::Nextendo::IsNextendoHost(host))) {" 2)
nextendo_write(sfdnsres.cpp "${dns}")

file(READ "${NEXTENDO_SOURCE}/src/core/hle/service/ssl/ssl_backend_openssl.cpp" tls)
string(PREPEND tls "#include \"nextendo_tls.h\"\n")
nextendo_replace(tls "        SSL_set_connect_state(ssl);"
    "        if (Eden::Nextendo::GuestEnabled() && !Eden::Nextendo::LoadGuestTrust(ssl)) return ResultInternalError;\n        SSL_set_connect_state(ssl);" 1)
nextendo_replace(tls "    Result SetHostName(const char* hostname) override {"
    [=[    Result SetHostName(const char* hostname) override {
        if (Eden::Nextendo::GuestEnabled()) {
            skip_cert_verification = false;
            return Eden::Nextendo::ConfigureGuestTls(ssl, hostname ? hostname : "") ?
                ResultSuccess : ResultInternalError;
        }]=] 1)
nextendo_replace(tls "        skip_cert_verification = (option == 0);"
    "        skip_cert_verification = !Eden::Nextendo::GuestEnabled() && (option == 0);" 1)
nextendo_replace(tls "    Result DoHandshake() override {"
    [=[    Result DoHandshake() override {
        if (Eden::Nextendo::GuestEnabled()) {
            skip_cert_verification = false;
            if (!Eden::Nextendo::EnforceGuestTls(ssl)) return ResultInternalError;
        }]=] 1)
# Do not allow an inherited debug environment to publish protected guest TLS traffic.
nextendo_replace(tls "    static void KeyLogCallback(const SSL* ssl, const char* line) {"
    "    static void KeyLogCallback(const SSL* ssl, const char* line) {\n        if (Eden::Nextendo::GuestEnabled()) return;" 1)
nextendo_write(ssl_backend_openssl.cpp "${tls}")

file(READ "${NEXTENDO_SOURCE}/src/core/hle/service/acc/acc.cpp" acc)
string(PREPEND acc "#include \"nextendo.h\"\n")
nextendo_replace(acc "rb.PushRaw<u64>(profile_manager->GetLastOpenedUser().Hash());"
    "rb.PushRaw<u64>(Eden::Nextendo::GuestEnabled() ? Eden::Nextendo::GuestPidForUser(profile_manager->GetLastOpenedUser().uuid) : profile_manager->GetLastOpenedUser().Hash());" 2)
nextendo_replace(acc "        *out_account_id = account_id.Hash();"
    "        *out_account_id = Eden::Nextendo::GuestEnabled() ? Eden::Nextendo::GuestPidForUser(account_id.uuid) : account_id.Hash();" 1)
nextendo_replace(acc "    Result LoadIdTokenCache() {"
    "    Result LoadIdTokenCache() {\n        if (Eden::Nextendo::GuestEnabled()) return ResultUnknown;" 1)
nextendo_replace(acc "    Result LoadIdTokenCacheDeprecated() {"
    "    Result LoadIdTokenCacheDeprecated() {\n        if (Eden::Nextendo::GuestEnabled()) return ResultUnknown;" 1)
# The pinned core fakes successful all-zero BAAS tokens. Never advertise those as a Nextendo
# authentication success. A signed NEX bearer is NOT itself a BAAS id_token; do not conflate them.
nextendo_replace(acc "    void LoadIdTokenCache(HLERequestContext& ctx) {"
    [=[    void LoadIdTokenCache(HLERequestContext& ctx) {
        if (Eden::Nextendo::GuestEnabled()) {
            IPC::ResponseBuilder rb{ctx, 3};
            rb.Push(ResultUnknown);
            rb.Push<u32>(0);
            return;
        }]=] 3)
nextendo_replace(acc "    void EnsureIdTokenCacheAsync(HLERequestContext& ctx) {"
    [=[    void EnsureIdTokenCacheAsync(HLERequestContext& ctx) {
        if (Eden::Nextendo::GuestEnabled()) {
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ResultUnknown);
            return;
        }]=] 1)
nextendo_write(acc.cpp "${acc}")

if(TARGET core)
    if(PS5_NATIVE)
        target_compile_definitions(core PRIVATE EDEN_NEXTENDO_NATIVE=1)
    endif()
    get_target_property(sources core SOURCES)
    list(REMOVE_ITEM sources hle/service/sockets/sfdnsres.cpp
        hle/service/ssl/ssl_backend_openssl.cpp hle/service/acc/acc.cpp)
    set_property(TARGET core PROPERTY SOURCES "${sources}")
    target_sources(core PRIVATE "${NEXTENDO_OUTPUT}/sfdnsres.cpp"
        "${NEXTENDO_OUTPUT}/ssl_backend_openssl.cpp" "${NEXTENDO_OUTPUT}/acc.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/nextendo.cpp")
    target_include_directories(core PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
endif()
