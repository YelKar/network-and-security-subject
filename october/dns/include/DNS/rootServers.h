#pragma once

#include <string_view>
#include <vector>

namespace dns
{

struct DomainServer
{
	std::string_view name;
	std::string_view ipv4;
};

const std::vector<DomainServer> ROOT_SERVERS = {
	{ .name = "a.root-servers.net", .ipv4 = "198.41.0.4" },
	{ .name = "b.root-servers.net", .ipv4 = "199.9.14.201" },
	{ .name = "c.root-servers.net", .ipv4 = "192.33.4.12" },
	{ .name = "d.root-servers.net", .ipv4 = "199.7.91.13" },
	{ .name = "e.root-servers.net", .ipv4 = "192.203.230.10" },
	{ .name = "f.root-servers.net", .ipv4 = "192.5.5.241" },
	{ .name = "g.root-servers.net", .ipv4 = "192.112.36.4" },
	{ .name = "h.root-servers.net", .ipv4 = "198.97.190.53" },
	{ .name = "i.root-servers.net", .ipv4 = "192.36.148.17" },
	{ .name = "j.root-servers.net", .ipv4 = "192.58.128.30" },
	{ .name = "k.root-servers.net", .ipv4 = "193.0.14.129" },
	{ .name = "l.root-servers.net", .ipv4 = "199.7.83.42" },
	{ .name = "m.root-servers.net", .ipv4 = "202.12.27.33" }
};

} // namespace dns