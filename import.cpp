
// grindtrick import pstream

#include <iostream>
#include <fstream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <pstream.h>

using namespace boost::property_tree;
using namespace std;

wstring to_markdown(wstring const & content)
{
	using namespace redi;

	vector<string> args;
	args.push_back("node");
	args.push_back("to_markdown.js");

 	typedef basic_pstream<wchar_t> wpstream;

	wpstream in("node", args, pstreambuf::pstdin | pstreambuf::pstdout);
	in << content;
	peof(in); // close stdin so client knows to finish

	wstring line;
	wstring markdown;
	while( getline(in, line) )
	{
		markdown = markdown + line + L"\n";
	}	

	return markdown;
}

class Attachment
{
public:
	wstring data_;
	wstring encoding_;
	wstring filename_;
};


Attachment resource(wptree const & pt)
{
	Attachment a;

	auto data_node = pt.get_child(L"data");

	a.data_ = data_node.get_value<wstring>();

	for(auto v: data_node)
	{
		if( v.first == L"<xmlattr>" )
		{
			for(auto v: v.second)
			{
				if( v.first == L"encoding" )
				{
					a.encoding_ = v.second.get_value<wstring>();
				}
			}
		}
	}

	auto opt_res_attr = pt.get_child_optional(L"resource-attributes" );
	if( opt_res_attr )
	{
		auto res_attr = *opt_res_attr;
		auto opt_fn = res_attr.get_optional<wstring>(L"file-name");
		if( opt_fn ) a.filename_ = *opt_fn;
	}

	return a;
}


void note(wptree const & pt)
{
	wstring title;
	wstring content;
	vector<wstring> tags;
	vector<Attachment> attachments;

	for(auto v: pt)
	{
		if( v.first == L"title" )
		{
			title = v.second.get_value<wstring>();
			wcout << title << '\n';
		}
		else if( v.first == L"content")
		{
			content = v.second.get_value<wstring>();
		}
		else if( v.first == L"tag")
		{
			tags.push_back( v.second.get_value<wstring>() );
		}
		else if( v.first == L"resource")
		{
			attachments.push_back( resource(v.second) );
		}
	}

	if( !tags.empty() )
	{
		wcout << "  tags: ";
		for(auto v: tags) wcout << v << ' ';
		wcout << '\n';
	}

	wcout << "content: " << to_markdown(content);
}

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

		for(auto v: pt.get_child(L"en-export"))
		{
			if( v.first == L"note" )
			{
				note(v.second);
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
