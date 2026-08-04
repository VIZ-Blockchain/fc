#pragma once
#include <boost/version.hpp>
#include <boost/asio.hpp>

// Boost 1.87 removed Asio's long-deprecated interface, including the
// io_service typedef.  The vendored websocketpp 0.8.2 copy (upstream dead
// since 2020) spells it that way in ~100 places.  Restoring the name as an
// alias keeps that copy diffable against upstream, and because the alias
// makes io_service and io_context the same type, ported first-party code can
// hand a plain io_context to websocketpp's init_asio(io_service*).
//
// Include this BEFORE any websocketpp header.  Only two translation units
// need it: fc/src/network/http/websocket.cpp and
// plugins/webserver/webserver_plugin.cpp.
#if BOOST_VERSION >= 108700
namespace boost { namespace asio {
    using io_service = io_context;
} }
#endif
