TESTS = LibTorrentTest
AUTOMAKE_OPTIONS = subdir-objects

check_PROGRAMS = $(TESTS)
LibTorrentTest_LDADD = \
	../src/libtorrent.la \
	../src/torrent/libsub_torrent.la \
	../src/torrent/data/libsub_torrentdata.la \
	../src/torrent/download/libsub_torrentdownload.la \
	../src/torrent/peer/libsub_torrentpeer.la \
	../src/data/libsub_data.la \
	../src/dht/libsub_dht.la \
	../src/net/libsub_net.la \
	../src/protocol/libsub_protocol.la \
	../src/download/libsub_download.la \
	../src/tracker/libsub_tracker.la \
	../src/utils/libsub_utils.la \
	../src/torrent/utils/libsub_torrentutils.la

LibTorrentTest_SOURCES = \
	helpers/expect_fd.h \
	helpers/expect_utils.h \
	helpers/mock_compare.h \
	helpers/mock_function.cc \
	helpers/mock_function.h \
	helpers/network.h \
	helpers/progress_listener.cc \
	helpers/progress_listener.h \
	helpers/test_fixture.cc \
	helpers/test_fixture.h \
	\
	../src/thread_disk.cc \
	../src/thread_disk.h \
	\
	rak/allocators_test.cc \
	rak/allocators_test.h \
	rak/ranges_test.cc \
	rak/ranges_test.h \
	data/chunk_list_test.cc \
	data/chunk_list_test.h \
	data/hash_check_queue_test.cc \
	data/hash_check_queue_test.h \
	data/hash_queue_test.cc \
	data/hash_queue_test.h \
	\
	net/test_socket_listen.cc \
	net/test_socket_listen.h \
	\
	protocol/test_request_list.cc \
	protocol/test_request_list.h \
	\
	torrent/net/test_address_info.cc \
	torrent/net/test_address_info.h \
	torrent/net/test_fd.cc \
	torrent/net/test_fd.h \
	torrent/net/test_socket_address.cc \
	torrent/net/test_socket_address.h \
	\
	torrent/utils/log_test.cc \
	torrent/utils/log_test.h \
	torrent/utils/option_strings_test.cc \
	torrent/utils/option_strings_test.h \
	torrent/utils/test_extents.cc \
	torrent/utils/test_extents.h \
	torrent/utils/test_log_buffer.cc \
	torrent/utils/test_log_buffer.h \
	torrent/utils/test_queue_buckets.cc \
	torrent/utils/test_queue_buckets.h \
	torrent/utils/test_uri_parser.cc \
	torrent/utils/test_uri_parser.h \
	torrent/utils/signal_bitfield_test.cc \
	torrent/utils/signal_bitfield_test.h \
	torrent/utils/thread_base_test.cc \
	torrent/utils/thread_base_test.h \
	\
	torrent/http_test.cc \
	torrent/http_test.h \
	torrent/object_test.cc \
	torrent/object_test.h \
	torrent/object_test_utils.cc \
	torrent/object_test_utils.h \
	torrent/object_static_map_test.cc \
	torrent/object_static_map_test.h \
	torrent/object_stream_test.cc \
	torrent/object_stream_test.h \
	torrent/tracker_controller_test.cc \
	torrent/tracker_controller_test.h \
	torrent/tracker_controller_features.cc \
	torrent/tracker_controller_features.h \
	torrent/tracker_controller_requesting.cc \
	torrent/tracker_controller_requesting.h \
	torrent/tracker_list_test.cc \
	torrent/tracker_list_test.h \
	torrent/tracker_list_features_test.cc \
	torrent/tracker_list_features_test.h \
	torrent/tracker_timeout_test.cc \
	torrent/tracker_timeout_test.h \
	tracker/tracker_http_test.cc \
	tracker/tracker_http_test.h \
	main.cc

LibTorrentTest_CXXFLAGS = $(CPPUNIT_CFLAGS)
LibTorrentTest_LDFLAGS = $(CPPUNIT_LIBS)  -ldl

AM_CPPFLAGS = -I$(srcdir) -I$(top_srcdir) -I$(top_srcdir)/src
