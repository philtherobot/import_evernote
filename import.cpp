
// grindtrick import pstream
// grindtrick import boost_locale

#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/locale/encoding.hpp>

#include <pstream.h>

using namespace boost::property_tree;
using namespace boost::locale::conv;
using namespace std;

wstring g_sphere_tag;

wstring to_markdown(wstring const & content)
{
	// TODO: stop saving this
	wofstream ifs("to_markdown_input");
	ifs << content;

	using namespace redi;

	vector<string> args;
	args.push_back("node");
	args.push_back("to_markdown.js");

	pstream in("node", args, pstreambuf::pstdin | pstreambuf::pstdout);

	string utf8 = utf_to_utf<char, wchar_t>(content.c_str());

	in << utf8;

	peof(in); // close stdin so client knows to finish

	// TODO: stop saving this
	ofstream of("to_markdown_result");

	string line;
	string markdown;
	while( getline(in, line) )
	{
		markdown = markdown + line + "\n";
		of << line << '\n';
	}	

	// TODO: to_markdown keeps &nbsp, which I do not care about
	// Replace them.

	return utf_to_utf<wchar_t, char>(markdown.c_str());
}

class Attachment
{
public:
	wstring data_;
	wstring encoding_;
	wstring filename_;
};

class Note
{
public:
	wstring title_;
	wstring content_;
	set<wstring> tags_;
	vector<Attachment> attachments_;
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

	auto opt_res_attr = pt.get_child_optional(L"resource-attributes");
	if( opt_res_attr )
	{
		auto res_attr = *opt_res_attr;
		auto opt_fn = res_attr.get_optional<wstring>(L"file-name");
		if( opt_fn ) a.filename_ = *opt_fn;
	}

	return a;
}

void write_note(Note const & n)
{
	set<wstring> full_tags_set(n.tags_);
	full_tags_set.insert(g_sphere_tag);
	full_tags_set.insert(L"imported");

	// TODO: project tag: pick first tag not g_sphere_of_life
	// TODO: make sure file name is clean for fielsystem
	string fn;
	fn = "inro imported " + from_utf(n.title_, "UTF-8") + ".md";
	wofstream os(fn);

	os << L"Sujet: " << n.title_ << '\n';

	// TODO: proper accented field name.
	os << L"Etiquettes: ";

	for(auto t: full_tags_set)
	{
		os << t << L' ';
	}
	os << '\n';

	os << '\n';

	os << n.content_;

	// TODO: save attachments
}

void note(wptree const & pt)
{
	Note n;

	for(auto v: pt)
	{
		if( v.first == L"title" )
		{
			n.title_ = v.second.get_value<wstring>();
		}
		else if( v.first == L"content")
		{
			wstring en_note = v.second.get_value<wstring>();
			// en_note is a full XML document.
			// The HTML of the note is inside the <en-note> tag.

			wptree en_note_ptree;
			wistringstream is(en_note);
			read_xml(is, en_note_ptree);

			auto enote = en_note_ptree.get_child(L"en-note");

			wostringstream os;
			write_xml(os, enote);
			
			n.content_ = to_markdown( os.str() );
		}
		else if( v.first == L"tag")
		{
			n.tags_.insert( v.second.get_value<wstring>() );
		}
		else if( v.first == L"resource")
		{
			n.attachments_.push_back( resource(v.second) );
		}
	}

	write_note(n);
}

int main(int argc, char ** argv)
{
	// Load the system locale.
	setlocale(LC_ALL, "");


	try
	{
		wptree pt;

		char const * fn = "INRO.enex";
		g_sphere_tag = L"inro";

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
