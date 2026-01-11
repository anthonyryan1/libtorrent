#ifndef RTORRENT_CORE_CURL_SHARE_H
#define RTORRENT_CORE_CURL_SHARE_H

#include <mutex>
#include <curl/curl.h>

namespace torrent::net {

class CurlShare {
public:
  CurlShare();
  ~CurlShare();

  CurlShare(const CurlShare&) = delete;
  CurlShare& operator=(const CurlShare&) = delete;

  CURLSH*             handle() const { return m_handle; }

  bool                is_initialized() const { return m_handle != nullptr; }

  // Returns true if successfully initialized
  void                initialize();
  void                cleanup();

private:
  static void         lock_function(CURL* handle, curl_lock_data data, curl_lock_access access, void* userptr);
  static void         unlock_function(CURL* handle, curl_lock_data data, void* userptr);

  std::mutex&         get_mutex(curl_lock_data data);

  CURLSH*             m_handle{nullptr};

  // Separate mutexes for different shared data types to reduce contention
  std::mutex          m_mutex_share;
  std::mutex          m_mutex_dns;
  std::mutex          m_mutex_ssl_session;
  std::mutex          m_mutex_connect;
};

} // namespace torrent::net

#endif
