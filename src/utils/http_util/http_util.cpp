/*
* HTTP utilities
* (C) 2013 Jack Lloyd
*
* Distributed under the terms of the Botan license
*/

#include <botan/http_util.h>
#include <botan/parsing.h>
#include <botan/hex.h>

#include <boost/asio.hpp>

namespace Botan {

namespace HTTP {

std::string url_encode(const std::string& in)
   {
   std::ostringstream out;

   for(auto c : in)
      {
      if(c >= 'A' && c <= 'Z')
         out << c;
      else if(c >= 'a' && c <= 'z')
         out << c;
      else if(c >= '0' && c <= '9')
         out << c;
      else if(c == '-' || c == '_' || c == '.' || c == '~')
         out << c;
      else
         out << '%' << hex_encode(reinterpret_cast<byte*>(&c), 1);
      }

   std::cout << "URL(" << in << ") = " << out.str();

   return out.str();
   }

Response http_sync(const std::string& verb,
                   const std::string& url,
                   const std::string& content_type,
                   const std::vector<byte>& body,
                   std::chrono::milliseconds timeout,
                   size_t allowable_redirects)
   {
   using namespace boost::asio::ip;

   const auto protocol_host_sep = url.find("://");
   if(protocol_host_sep == std::string::npos)
      throw std::runtime_error("Invalid URL " + url);
   const std::string protocol = url.substr(0, protocol_host_sep);

   const std::string hostname = url.substr(protocol_host_sep+3, url.find('/', protocol_host_sep + 3));

   std::string loc = url.substr(url.find('/', protocol_host_sep), std::string::npos);
   if(loc == "")
      loc = "/";

   if(protocol != "http")
      throw std::runtime_error("Unsupported protocol " + protocol);

   tcp::iostream sock;

   if(timeout.count())
      sock.expires_from_now(boost::posix_time::milliseconds(timeout.count()));

   sock.connect(hostname, protocol);
   if(!sock)
      throw std::runtime_error("Connection to " + hostname + " / " + protocol + " failed");

   std::ostringstream outbuf;

   outbuf << verb << " " << loc << " HTTP/1.0\r\n";
   outbuf << "Host: " << hostname << "\r\n";

   if(verb == "GET")
      {
      outbuf << "Accept: */*\r\n";
      outbuf << "Cache-Control: no-cache\r\n";
      }
   else if(verb == "POST")
      outbuf << "Content-Length: " << body.size() << "\r\n";

   if(content_type != "")
      outbuf << "Content-Type: " << content_type << "\r\n";
   outbuf << "Connection: close\r\n\r\n";
   outbuf.write(reinterpret_cast<const char*>(&body[0]), body.size());

   sock << outbuf.str();
   sock.flush();

   std::string line1;
   std::getline(sock, line1);
   if(!sock)
      throw std::runtime_error("No response");

   std::stringstream response_stream(line1);
   std::string http_version;
   unsigned int status_code;
   std::string status_message;

   response_stream >> http_version >> status_code;

   std::getline(response_stream, status_message);

   if(!response_stream || http_version.substr(0,5) != "HTTP/")
      throw std::runtime_error("Not an HTTP response");

   std::map<std::string, std::string> headers;
   std::string header_line;
   while (std::getline(sock, header_line) && header_line != "\r")
      {
      auto sep = header_line.find(": ");
      if(sep == std::string::npos || sep > header_line.size() - 2)
         throw std::runtime_error("Invalid HTTP header " + header_line);
      const std::string key = header_line.substr(0, sep);
      const std::string val = header_line.substr(sep + 2, std::string::npos);
      headers[key] = val;
      }

   if(status_code == 301 && headers.count("Location"))
      {
      if(allowable_redirects == 0)
         throw std::runtime_error("HTTP redirection count exceeded");
      return GET_sync(headers["Location"], timeout, allowable_redirects - 1);
      }

   // Use Content-Length if set
   std::vector<byte> resp_body;
   std::vector<byte> buf(4096);
   while(sock.good())
      {
      sock.read(reinterpret_cast<char*>(&buf[0]), buf.size());
      resp_body.insert(resp_body.end(), &buf[0], &buf[sock.gcount()]);
      }

   return Response(status_code, status_message, resp_body, headers);
   }

Response GET_sync(const std::string& url,
                  std::chrono::milliseconds timeout,
                  size_t allowable_redirects)
   {
   return http_sync("GET", url, "", std::vector<byte>(),
                    timeout, allowable_redirects);
   }

Response POST_sync(const std::string& url,
                   const std::string& content_type,
                   const std::vector<byte>& body,
                   std::chrono::milliseconds timeout,
                   size_t allowable_redirects)
   {
   return http_sync("POST", url, content_type, body,
                    timeout, allowable_redirects);
   }

std::future<Response> GET_async(const std::string& url)
   {
   return std::async(std::launch::async, GET_sync, url,
                     std::chrono::seconds(5), 1);
   }

}

}
