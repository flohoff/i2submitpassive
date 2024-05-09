
#include <iostream>
#include <fstream>
#include <string>
#include <regex>

#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <nlohmann/json.hpp>
#include <sys/utsname.h>

namespace po = boost::program_options;
using json = nlohmann::json;


// Try to find the line
//
// const NodeName = "brick.b1i.zz.de"
//
// from /etc/icinga2/constants.conf
//
std::string find_nodename(void ) {
	std::ifstream constants;
	constants.open("/etc/icinga2/constants.conf");

	if (!constants.is_open())
		return "";

	std::string line;
	std::regex r("const\\s+nodename\\s*=\\s*\"([^\"]+)\"", std::regex_constants::icase);

	while(std::getline(constants, line)) {
		std::smatch sm;
		if (regex_search(line, sm, r)) {
			return sm[1];
		}
	}

	std::cout << std::endl;

	return "";
}

int main(int argc, char **argv) {
	bool	t_verbose=false;

        po::options_description         desc("Allowed options");
        desc.add_options()
                ("help,h",	"produce help message")
                ("verbose,v",	po::bool_switch(&t_verbose), "Verbose")
		("certdir",	po::value<std::string>()->default_value("/var/lib/icinga2/certs"),	"Icinga2 certificate dir")
		("api",		po::value<std::string>()->default_value("https://localhost:5665"),	"Icinga2 api")
                ("nodename",	po::value<std::string>(),		"Icinga2 agent nodename")
                ("host",	po::value<std::string>(),		"Icinga2 host name")
                ("service",	po::value<std::string>()->required(),	"Icinga2 service name")
		("output",	po::value<std::string>()->required(),	"Plugin output")
		("status",	po::value<std::string>()->default_value("ok"),	"ok, critical, warning, unknown")
        ;

        po::variables_map vm;
        try {
                po::store(po::parse_command_line(argc, argv, desc), vm);
                po::notify(vm);
        } catch(const boost::program_options::error& e) {
                std::cerr << "Error: " << e.what() << "\n";
                std::cout << desc << std::endl;
                exit(-1);
        }

	//
	// Try to find the nodename from the icinga2 config
	// and use command line based nodename to override
	//
	// This is needed to access the correct certificate
	// and keyfile to authenticate against the API
	//
	std::string nodename=find_nodename();
	if (vm.count("nodename")) 
		nodename=vm["nodename"].as<std::string>();

	std::string certfile=boost::str(boost::format("%1%/%2%.crt") % vm["certdir"].as<std::string>() % nodename);
	std::string keyfile=boost::str(boost::format("%1%/%2%.key") % vm["certdir"].as<std::string>() % nodename);

	//
	// Create post data needed for passive submission
	//
	std::string hostname=nodename;
	if (vm.count("host")) {
		hostname=vm["host"].as<std::string>();
	}

	if (hostname.length() == 0) {
		std::cerr << "Need hostname for submission" << std::endl;
		exit(1);
	}

	json j;

	std::string filter=boost::str(boost::format("host.name==\"%1%\" && service.name==\"%2%\"") 
			% hostname % vm["service"].as<std::string>());

	// Convert OK, WARNING, CRITICAL and UNKNOWN to numerical values for API
	int status=0;
	std::string statusstr=vm["status"].as<std::string>();
	boost::algorithm::to_lower(statusstr);
	if (statusstr == "ok")
		status=0;
	else if (statusstr == "warning")
		status=1;
	else if (statusstr == "critical")
		status=2;
	else if (statusstr == "unknown")
		status=3;
	else {
		std::cerr << "Unknown status " << vm["status"].as<std::string>() << std::endl;
		exit(1);
	}

	j["type"]="Service";
	j["filter"]=filter;
	j["exit_status"]=status;
	j["plugin_output"]=vm["output"].as<std::string>();

	std::string postdata=j.dump();

	if (t_verbose)
		std::cout << "Post data: " << postdata << std::endl;

	try {
		curlpp::Easy req;
		std::list<std::string>	header;
		header.push_back("Accept: application/json");

		std::string uri=boost::str(boost::format("%1%/v1/actions/process-check-result") % vm["api"].as<std::string>());
		req.setOpt<curlpp::options::Url>(uri);

		req.setOpt<curlpp::options::Post>(1);
		req.setOpt<curlpp::options::HttpHeader>(header);
		req.setOpt<curlpp::options::SslVerifyHost>(0);
		req.setOpt<curlpp::options::SslVerifyPeer>(0);
		req.setOpt<curlpp::options::SslCert>(certfile);
		req.setOpt<curlpp::options::SslKey>(keyfile);
		req.setOpt(new curlpp::options::PostFields(postdata));
		req.setOpt(new curlpp::options::PostFieldSize(postdata.length()));

		if (t_verbose)
			req.setOpt(curlpp::options::Verbose(true));

		req.perform();

	} catch(curlpp::LogicError &e) {
		std::cout << "Logic error " << e.what() << std::endl;
	} catch(curlpp::RuntimeError &e) {
		std::cout << "Runtime error " << e.what() << std::endl;
	}
}
