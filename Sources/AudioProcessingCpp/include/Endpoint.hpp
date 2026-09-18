#pragma once
#include <string>

// Where to send packets. Discovery later only has to produce one of these;
// the media path should not care whether it came from argv, Bonjour, or a QR.
class Endpoint {
public:
  Endpoint(const char *host, const char *port)
      : host_(host ? host : ""), port_(port ? port : "") {}

  const char *host() const { return host_.c_str(); }
  const char *port() const { return port_.c_str(); }

private:
  std::string host_;
  std::string port_;
};
