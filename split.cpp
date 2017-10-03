
#include <iostream>
#include <iomanip>
#include <fstream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

using namespace boost::property_tree;
using namespace std;

int main(int argc, char ** argv)
{
	// Load the system locale.
	setlocale(LC_ALL, "");


	try
	{
		wptree pt;

		char const * fn = "INRO.enex";

		if( argc >= 2 ) fn = argv[1];

		wifstream enex_file(fn);
		read_xml(enex_file, pt);

		int counter = 1;

		for(auto v: pt.get_child(L"en-export"))
		{
			if( v.first == L"note" )
			{
				ostringstream fn_os;
				fn_os << setw(5) << setfill('0')  << counter << ".xml";

				wstring note_xml;

				wofstream os(fn_os.str());
				os.exceptions(~ios::goodbit);

				write_xml(os, v.second);

				++counter;
			}
		}
	}	
	catch(exception const & ex)
	{
		cerr << "exception: " << ex.what() << '\n';
		return 1;
	}
	
	return 0;
}
