#include "config.h"

#include "curl_share.h"

#include <string>

#include "torrent/exceptions.h"

namespace torrent::net {

CurlShare::CurlShare() = default;

CurlShare::~CurlShare() {
  cleanup();
}

void
CurlShare::initialize() {
  if (m_handle != nullptr)
    throw torrent::internal_error("CurlShare::initialize() called when already initialized.");

  m_handle = curl_share_init();

  if (m_handle == nullptr)
    throw torrent::internal_error("CurlShare::initialize() curl_share_init() returned nullptr.");

  CURLSHcode code;

  // Set up lock/unlock callbacks
  code = curl_share_setopt(m_handle, CURLSHOPT_LOCKFUNC, &CurlShare::lock_function);
  if (code != CURLSHE_OK)
    throw torrent::internal_error("CurlShare::initialize() failed to set CURLSHOPT_LOCKFUNC: " + std::string(curl_share_strerror(code)));

  code = curl_share_setopt(m_handle, CURLSHOPT_UNLOCKFUNC, &CurlShare::unlock_function);
  if (code != CURLSHE_OK)
    throw torrent::internal_error("CurlShare::initialize() failed to set CURLSHOPT_UNLOCKFUNC: " + std::string(curl_share_strerror(code)));

  code = curl_share_setopt(m_handle, CURLSHOPT_USERDATA, this);
  if (code != CURLSHE_OK)
    throw torrent::internal_error("CurlShare::initialize() failed to set CURLSHOPT_USERDATA: " + std::string(curl_share_strerror(code)));

  // DNS cache, ignores TTL, limited by CURLOPT_DNS_CACHE_TIMEOUT (default 60 seconds)
  code = curl_share_setopt(m_handle, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
  if (code != CURLSHE_OK)
    throw torrent::internal_error("CurlShare::initialize() failed to share CURL_LOCK_DATA_DNS: " + std::string(curl_share_strerror(code)));

  // TLS resumption (session id, tickets, etc.), limited to 24 hour (not configurable)
  code = curl_share_setopt(m_handle, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
  if (code != CURLSHE_OK)
    throw torrent::internal_error("CurlShare::initialize() failed to share CURL_LOCK_DATA_SSL_SESSION: " + std::string(curl_share_strerror(code)));

  // Reuse connections from same tracker, limited by lesser of tracker Keep-Alive or CURLOPT_MAXAGE_CONN (default 118 seconds)
  code = curl_share_setopt(m_handle, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
  if (code != CURLSHE_OK)
    throw torrent::internal_error("CurlShare::initialize() failed to share CURL_LOCK_DATA_CONNECT: " + std::string(curl_share_strerror(code)));
}

void
CurlShare::cleanup() {
  if (m_handle != nullptr) {
    curl_share_cleanup(m_handle);
    m_handle = nullptr;
  }
}

void
CurlShare::lock_function([[maybe_unused]] CURL* handle, curl_lock_data data, [[maybe_unused]] curl_lock_access access, void* userptr) {
  auto* share = static_cast<CurlShare*>(userptr);
  share->get_mutex(data).lock();
}

void
CurlShare::unlock_function([[maybe_unused]] CURL* handle, curl_lock_data data, void* userptr) {
  auto* share = static_cast<CurlShare*>(userptr);
  share->get_mutex(data).unlock();
}

std::mutex&
CurlShare::get_mutex(curl_lock_data data) {
  switch (data) {
  case CURL_LOCK_DATA_SHARE:
    return m_mutex_share;
  case CURL_LOCK_DATA_DNS:
    return m_mutex_dns;
  case CURL_LOCK_DATA_SSL_SESSION:
    return m_mutex_ssl_session;
  case CURL_LOCK_DATA_CONNECT:
    return m_mutex_connect;
  default:
    throw torrent::internal_error("CurlShare::get_mutex() called with unexpected curl_lock_data type: " + std::to_string(static_cast<int>(data)));
  }
}

} // namespace torrent::net
